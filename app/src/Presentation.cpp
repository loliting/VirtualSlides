#include "Presentation.hpp"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtGui/QResizeEvent>

#include <string>
#include <sstream>

#include <zip.h>
#include "third-party/RapidXml/rapidxml.hpp"
#include "third-party/RapidXml/rapidxml_print.hpp"

// Size of unzip buffor
#define BUFFOR_SZ 1024
#define TEXT_NODE_SZ sizeof("<Text>")

using namespace rapidxml;

static void parseXmlDimensions(xml_node<char>* xmlNode, PresentationElement* e) {
    auto getDim = [=](QByteArray dimName) -> qreal {
        xml_attribute<char>* attrib = nullptr;
        QString dimStr = nullptr;
        if((attrib = xmlNode->first_attribute(dimName, 0UL, false)) != nullptr)
            dimStr = QString(attrib->value());
        else if((attrib = xmlNode->first_attribute(dimName, 1UL, false)) != nullptr)
            dimStr = QString(attrib->value());            
        else{
            qWarning() << xmlNode->name() << "does not contain" << dimName << "attribute.";
            return 0.0f;
        }

        if(dimStr.isEmpty()){
            qWarning() << xmlNode->name() << "contains" << dimName << "attribute, but it's empty";
            return 0.0f;
        }

        bool ok;
        qreal dim = dimStr.toDouble(&ok) / 100;
        if(!ok){
            qWarning() << "Failed to convert" << dimStr << "to double type";
            return 0.0f;
        }
        return dim;
    };

    e->setX(getDim("x"));
    e->setY(getDim("y"));
    e->setWidth(getDim("width"));
    e->setHeight(getDim("height"));
}

void PresentationElement::recalculateRealDim() {
    if(m_widget == nullptr){
        return;
    }

    QWidget* parent = m_widget->parentWidget();
    if(parent == nullptr){
        return;
    }

    m_widget->resize(width() * parent->width(),
        height() * parent->height()
    );
    m_widget->move(x() * parent->width() - m_widget->width() / 2,
        y() * parent->height() - m_widget->height() / 2
    );
}

PresentationElement::PresentationElement() {
    connect(this, SIGNAL(widgetSet(void)), this, SLOT(recalculateRealDim(void)));
    connect(this, SIGNAL(sizeChanged(void)), this, SLOT(recalculateRealDim(void)));
    connect(this, SIGNAL(positionChanged(void)), this, SLOT(recalculateRealDim(void)));
}

PresentationElement::PresentationElement(qreal x, qreal y, qreal height, qreal width, QWidget *w) {
    PresentationElement();
    m_pos = QPointF(x, y);
    m_size = QSizeF(width, height);
    
    m_widget = w;
    setWidget(m_widget);
}

void PresentationElement::setWidget(QWidget* w) {
    assert(m_widget == nullptr);
    m_widget = w;

    QWidget* parent = m_widget->parentWidget();
    assert(parent != nullptr);

    connect(parent, SIGNAL(resize(int, int)), this, SLOT(recalculateRealDim(void)));
    w->show();
}

PresentationSlide::PresentationSlide(QColor bg) {
    setAutoFillBackground(true);
    setPalette(QPalette(bg));
}

PresentationSlide::PresentationSlide(QPixmap bg) {
    setPixmap(bg);
    setScaledContents(true);
}

PresentationSlide::~PresentationSlide() {
    for(auto element : m_elements){
        element->deleteLater();
    }
}

void PresentationSlide::resizeEvent(QResizeEvent *event) {
    emit resize(event->size().width(), event->size().height());
    QLabel::resizeEvent(event);
}

void Presentation::decompressArchive(QString path) {
    zip_t* zipArchive = nullptr;
    zip_file_t* zf;
    zip_stat_t zStat;
    int zipErrorCode = 0;

    QByteArray cPath = path.toUtf8();
    if((zipArchive = zip_open(cPath.data(), ZIP_RDONLY, &zipErrorCode)) == nullptr){
        zip_error_t error;
        zip_error_init_with_code(&error, zipErrorCode);
        QString exceptionString = "Failed to open '" + path + "': "
            + zip_error_strerror(&error);
        zip_error_fini(&error);
        throw PresentationException(exceptionString);
    }

    QDir tmpDir(m_tmpDir.path());

    size_t zEntriesCount = zip_get_num_entries(zipArchive, 0);
    for(size_t i = 0; i < zEntriesCount; ++i){
        if(zip_stat_index(zipArchive, i, 0, &zStat) == -1){
            zip_error_t* error = zip_get_error(zipArchive);
            QString exceptionString = "Failed to decompress '" + path + "': "
                + zip_error_strerror(error);
            zip_close(zipArchive);
            throw PresentationException(exceptionString);
        }
        QString fileName = QString(zStat.name);
        if(fileName[fileName.length() - 1] == '/'){
            if(!tmpDir.mkpath(fileName)){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }
        }
        else{
            QFile file(m_tmpDir.path() + "/" + fileName);
            if(file.open(QIODevice::WriteOnly) == false){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "': " + file.errorString());
            }
            zf = zip_fopen_index(zipArchive, i, 0);
            if(zf == nullptr){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }

            char buffor[BUFFOR_SZ];
            ssize_t sum = 0;
            ssize_t length;
            while(sum < zStat.size){
                if((length = zip_fread(zf, buffor, BUFFOR_SZ)) == -1){
                    file.close();
                    zip_close(zipArchive);
                    throw PresentationException("Failed to decompress '" + path + "'");
                }
                file.write(buffor, length);
                sum += length;
            }
            file.close();
        }
    }
    zip_close(zipArchive);
}

void Presentation::parseRootXml() {
    QFile rootXml(getFilePath("root.xml"));

    if(rootXml.open(QIODevice::ReadOnly) == false){
        throw PresentationException("File root.xml does not exists inside the archive");
    }
    
    QByteArray ba = rootXml.readAll();
    
    xml_document<char> xmlDoc;
    try{
        xmlDoc.parse<parse_trim_whitespace | parse_normalize_whitespace>(ba.data());
    }
    catch(parse_error& e){
        throw PresentationException("Failed to parse root.xml: " + QString(e.what()));
    }

    xml_node<char>* rootNode = xmlDoc.first_node("Presentation", 0UL, false);
    if(rootNode == nullptr){
        QString exceptionStr = "Failed to parse root.xml: ";
        exceptionStr += "\"<Presentation>\" Node does not exist";
        throw PresentationException(exceptionStr);
    }

    xml_attribute<char>* titleAttribute;
    titleAttribute = rootNode->first_attribute("title", 0UL, false);
    if(titleAttribute != nullptr){
        m_title = QString(titleAttribute->value());
    }


    xml_node<char>* slideNode = rootNode->first_node("Slide", 0UL, false);
    xml_attribute<char>* bgAttribute;
    
    xml_node<char>* tmpNode;
    xml_node<char>* textNode;

    xml_attribute<char>* srcAttribute;
    xml_attribute<char>* vmIdAttribute;

    if(slideNode == nullptr){
        QString exceptionStr = "Failed to parse root.xml: ";
        exceptionStr += "\"<Presentation>\" node have to contain at least one \"<Slide>\" node";
        throw PresentationException(exceptionStr);
    }
    while(slideNode){
        PresentationSlide* slide = nullptr;

        bgAttribute = slideNode->first_attribute("bg", 0UL, false);
        if(bgAttribute){
            if(isFileValid(bgAttribute->value()))
                slide = new PresentationSlide(QPixmap(getFilePath(bgAttribute->value())));
            else if(QColor::isValidColorName(bgAttribute->value()))
                slide = new PresentationSlide(QColor(bgAttribute->value()));
            else
                slide = new PresentationSlide();
        }
        else{
            slide = new PresentationSlide();
        }
        
        tmpNode = slideNode->first_node(nullptr, 0UL, false);
        while(tmpNode){
            QString nodeStr(tmpNode->name());
            nodeStr = nodeStr.toLower();
            if(nodeStr == QString("TextBox").toLower()){
                textNode = tmpNode->first_node("Text", 0UL, false);
                if(textNode){
                    std::string text = (std::stringstream() << *textNode).str();
                    text = text.substr(TEXT_NODE_SZ, text.length() - TEXT_NODE_SZ * 2);

                    QLabel* w = new QLabel(slide);
                    w->setTextFormat(Qt::RichText);
                    w->setText(QString::fromStdString(text));
                    PresentationElement* tE = new PresentationElement();
                    tE->setWidget(w);
                    parseXmlDimensions(tmpNode, tE);

                    slide->m_elements.append(tE);
                }
                else{
                    qWarning() << "TextBox node does not contain \"Text\" subnode.";
                }
            }
            else if(nodeStr == QString("Image").toLower()){
                PresentationElement* tE = new PresentationElement();
                QLabel* w = new QLabel(slide);

                QPixmap img;
                srcAttribute = tmpNode->first_attribute("src", 0UL, false);
                if(srcAttribute == nullptr){
                    qWarning() << "Image node does not contain \"src\" attribute";
                }
                else if(isFileValid(srcAttribute->value())){
                    img = QPixmap(getFilePath(srcAttribute->value()));
                }
                else{
                    qWarning() << "Image src: \"" + QString(srcAttribute->value()) + "\" is not valid";
                }

                w->setScaledContents(true);
                w->setPixmap(img);

                tE->setWidget(w);
                parseXmlDimensions(tmpNode, tE);

                slide->m_elements.append(tE);
            }
            else if(nodeStr == QString("vm").toLower()){
                vmIdAttribute = tmpNode->first_attribute("id", 0UL, false);
                QString vmId = vmIdAttribute ? vmIdAttribute->value() : nullptr; 
                VirtualMachine* vm = m_vmManager->getVirtualMachine(vmId);

                if(vm){
                    PresentationElement* tE = new PresentationElement();
                    VirtualMachineWidget* w = new VirtualMachineWidget(vm, slide);
                    tE->setWidget(w);
                    parseXmlDimensions(tmpNode, tE);

                    slide->m_elements.append(tE);
                }
                else{
                    qWarning() << "vm node does not contain \"id\" attribute.";
                }
            }
            else{
                qWarning() << "Unknown node: <" + nodeStr + ">. Ignoring.";
            }
            tmpNode = tmpNode->next_sibling(nullptr, 0UL, false);
        }
        m_slides.append(slide);
        slideNode = slideNode->next_sibling("Slide", 0UL, false);
    }

    rootXml.close();
}

Presentation::Presentation(QString path) {
    path = QFileInfo(path).absoluteFilePath();
    m_tmpDir.setAutoRemove(false);

    if(!m_tmpDir.isValid()){
        throw PresentationException(m_tmpDir.errorString());
    }

    try{
        decompressArchive(path);
        m_vmManager = new VirtualMachineManager(getFilePath("vms.xml"), this);
        m_netManager = new NetworkManager(getFilePath("nets.xml"));
        m_netManager->setVirtualMachineManager(m_vmManager);
        m_vmManager->setNetworkManager(m_netManager);
        parseRootXml();
        
    }
    catch(PresentationException &e){
        m_tmpDir.remove();
        throw;
    }
}

Presentation::~Presentation() {
    m_tmpDir.remove();
    for(auto slide : m_slides){
        delete slide;
    }
    delete m_netManager;
    delete m_vmManager;
}

bool Presentation::isFileValid(QString path) {
    assert(m_tmpDir.isValid() == true);

    path = m_tmpDir.path() + "/" + path;
    QFileInfo fi(path);
    if(fi.exists() == false){
        return false;
    }

    QString absoluteTmpDirPath = m_tmpDir.path();
    absoluteTmpDirPath = QFileInfo(absoluteTmpDirPath).absoluteFilePath();
    if(fi.absoluteFilePath().contains(absoluteTmpDirPath) == false){
        return false;
    }

    return true;
}

QString Presentation::getFilePath(QString path){
    if(isFileValid(path) == false){
        return nullptr;
    }

    QFileInfo fi(m_tmpDir.path() + "/" + path);

    return fi.absoluteFilePath();
}