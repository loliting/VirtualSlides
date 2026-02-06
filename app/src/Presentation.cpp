#include "Presentation.hpp"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtGui/QResizeEvent>

#include <string>
#include <sstream>

#include <zip.h>
#include "third-party/RapidXml/rapidxml.hpp"
#include "third-party/RapidXml/rapidxml_print.hpp"

#include "VirtualMachine.hpp"
#include "Network.hpp"
#include "VirtualMachineWidget.hpp"

#define BUFFOR_SZ 1024 // Unzip buffor size

using namespace rapidxml;
using namespace nlohmann;


static void parseXmlDimensions(xml_node<char>* xmlNode, PresentationElement* e) {
    auto getDim = [=](QByteArray dimName) -> qreal {
        xml_attribute<char>* attrib = nullptr;
        QString dimStr = nullptr;
        if((attrib = xmlNode->first_attribute(dimName, 0UL, false)) != nullptr)
            dimStr = QString(attrib->value());
        else if((attrib = xmlNode->first_attribute(dimName, 1UL, false)) != nullptr)
            dimStr = QString(attrib->value());            
        else {
            qWarning() << xmlNode->name() << "does not contain" << dimName << "attribute.";
            return 0.0f;
        }

        if(dimStr.isEmpty()) {
            qWarning() << xmlNode->name() << "contains" << dimName << "attribute, but it's empty";
            return 0.0f;
        }

        bool ok;
        qreal dim = dimStr.toDouble(&ok) / 100;
        if(!ok) {
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
    if(m_widget == nullptr) {
        return;
    }

    QWidget* parent = m_widget->parentWidget();
    if(parent == nullptr) {
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

PresentationElement* PresentationElement::create(xml_node<char>* node,
    PresentationSlide* slide, Presentation* pres)
{
    PresentationElement* presentationElement = nullptr;
    QString type(node->name());
    type = type.toLower();
    if(type == "box") {
        xml_node<char>* textNode = node->first_node("text", 0UL, false);
        xml_node<char>* htmlNode = node->first_node("html", 0UL, false);
        if(textNode) {
            QLabel* w = new QLabel(slide);
            w->setTextFormat(Qt::TextFormat::PlainText);
            w->setText(textNode->value());

            presentationElement = new PresentationElement();
            parseXmlDimensions(node, presentationElement);
            presentationElement->setWidget(w);
        }
        else if(htmlNode) {
            std::string text = (std::stringstream() << *htmlNode).str();
            QLabel* w = new QLabel(slide);
            w->setTextFormat(Qt::TextFormat::RichText);
            w->setText(QString::fromStdString(text));

            presentationElement = new PresentationElement();
            parseXmlDimensions(node, presentationElement);
            presentationElement->setWidget(w);
        }
        else
            qWarning() << "box node contains neither \"text\", nor \"html\" subnode.";
    }
    else if(type == "image") {
        QLabel* w = new QLabel(slide);
        w->setScaledContents(true);
        
        QPixmap img;
        xml_attribute<char>* srcAttr = node->first_attribute("src", 0UL, false);
        if(srcAttr == nullptr)
            qWarning() << "Image node does not contain \"src\" attribute";
        else if(pres->isFileValid(srcAttr->value()))
            img = QPixmap(pres->getFilePath(srcAttr->value()));
        else
            qWarning() << "Image src: \"" + QString(srcAttr->value()) + "\" is not valid";
        w->setPixmap(img);

        presentationElement = new PresentationElement();
        parseXmlDimensions(node, presentationElement);
        presentationElement->setWidget(w);
    }
    else if(type == "vm") {
        xml_attribute<char>* vmIdAttr = node->first_attribute("id", 0UL, false);
        QString vmId = vmIdAttr ? vmIdAttr->value() : nullptr; 
        VirtualMachine* vm = pres->getVirtualMachine(vmId);

        if(vm) {
            VirtualMachineWidget* w = new VirtualMachineWidget(vm, slide);
            
            presentationElement = new PresentationElement();
            presentationElement->setWidget(w);
            parseXmlDimensions(node, presentationElement);
        }
        else
            qWarning() << "vm node does not contain \"id\" attribute.";
    }
    else
        qWarning() << "Unknown node type: <" + type + ">. Ignoring.";

    return presentationElement;
}

PresentationSlide::PresentationSlide(xml_node<char>* node, Presentation* pres) {
    setAutoFillBackground(true);
    setScaledContents(true);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    xml_attribute<char>* bgAttribute = node->first_attribute("bg", 0UL, false);
    setPalette(QPalette(QColor("white")));
    if(bgAttribute) {
        if(pres->isFileValid(bgAttribute->value()))
            setPixmap(QPixmap(pres->getFilePath(bgAttribute->value())));
        else if(QColor::isValidColorName(bgAttribute->value()))
            setPalette(QPalette(QColor(bgAttribute->value())));
    }
     
    xml_node<char>* elementNode = node->first_node(nullptr, 0UL, false);
    while(elementNode) {
        PresentationElement* presentationElement = PresentationElement::create(elementNode, this, pres);
        if(presentationElement)
            m_elements.append(presentationElement);
        elementNode = elementNode->next_sibling(nullptr, 0UL, false);
    }    
}

PresentationSlide::~PresentationSlide() {
    for(auto element : m_elements)
        element->deleteLater();
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
    if((zipArchive = zip_open(cPath.data(), ZIP_RDONLY, &zipErrorCode)) == nullptr) {
        zip_error_t error;
        zip_error_init_with_code(&error, zipErrorCode);
        QString exceptionString = "Failed to open '" + path + "': "
            + zip_error_strerror(&error);
        zip_error_fini(&error);
        throw PresentationException(exceptionString);
    }

    QDir tmpDir(m_tmpDir.path());

    size_t zEntriesCount = zip_get_num_entries(zipArchive, 0);
    for(size_t i = 0; i < zEntriesCount; ++i) {
        if(zip_stat_index(zipArchive, i, 0, &zStat) == -1) {
            zip_error_t* error = zip_get_error(zipArchive);
            QString exceptionString = "Failed to decompress '" + path + "': "
                + zip_error_strerror(error);
            zip_close(zipArchive);
            throw PresentationException(exceptionString);
        }
        QString fileName = QString(zStat.name);
        if(fileName[fileName.length() - 1] == '/') {
            if(!tmpDir.mkpath(fileName)) {
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }
        }
        else {
            QFile file(m_tmpDir.path() + "/" + fileName);
            if(file.open(QIODevice::WriteOnly) == false) {
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "': " + file.errorString());
            }
            zf = zip_fopen_index(zipArchive, i, 0);
            if(zf == nullptr) {
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }

            char buffor[BUFFOR_SZ];
            ssize_t sum = 0;
            ssize_t length;
            while(sum < zStat.size) {
                if((length = zip_fread(zf, buffor, BUFFOR_SZ)) == -1) {
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

    if(rootXml.open(QIODevice::ReadOnly) == false)
        throw PresentationException("File root.xml does not exists inside the archive");
    
    QByteArray ba = rootXml.readAll();
    
    xml_document<char> xmlDoc;
    try {
        xmlDoc.parse<parse_trim_whitespace | parse_normalize_whitespace>(ba.data());
    }
    catch(parse_error& e) {
        throw PresentationException("Failed to parse root.xml: " + QString(e.what()));
    }

    xml_node<char>* rootNode = xmlDoc.first_node("Presentation", 0UL, false);
    if(rootNode == nullptr) {
        QString exceptionStr = "Failed to parse root.xml: ";
        exceptionStr += "\"<Presentation>\" Node does not exist";
        throw PresentationException(exceptionStr);
    }

    xml_attribute<char>* titleAttribute = rootNode->first_attribute("title", 0UL, false);
    if(titleAttribute != nullptr)
        m_title = QString(titleAttribute->value());


    xml_node<char>* slideNode = rootNode->first_node("Slide", 0UL, false);
    if(slideNode == nullptr) {
        QString exceptionStr = "Failed to parse root.xml: ";
        exceptionStr += "\"<Presentation>\" node have to contain at least one \"<Slide>\" node";
        throw PresentationException(exceptionStr);
    }

    while(slideNode) {
        PresentationSlide* slide = new PresentationSlide(slideNode, this);
        m_slides.append(slide);

        slideNode = slideNode->next_sibling("Slide", 0UL, false);
    }

    rootXml.close();
}

void Presentation::parseVirtualMachines(json &vmsObj) {
    for(auto &vmObj : vmsObj) {
        try {
            VirtualMachine* vm = nullptr;
            vm = new VirtualMachine(vmObj, this);

            if(vm->m_id.isEmpty()) {
                qWarning("virt-env.jsonc: Found virtual machine object with empty ID string. Ignoring...");

                delete vm;
                continue;
            }

            if(m_virtualMachines.contains(vm->m_id)) {
                qWarning("virt-env.jsonc: virtual machine %s already exist! Ignoring duplicate...", vm->m_id.toUtf8().data());

                delete vm;
                continue;
            }

            m_virtualMachines[vm->m_id] = vm;
        }
        catch(VirtualMachineException &e) {
            qWarning("virt-env.jsonc: parsing virtual machine object failed: %s. Ignoring...", e.what());
        }
    }
}

void Presentation::parseNetworks(json &networksObj) {
    for(auto &netObj : networksObj) {
        try {
            Network* net = nullptr;
            net = new Network(netObj);

            if(net->m_id.isEmpty()) {
                qWarning("virt-env.jsonc: Found network object with empty ID string. Ignoring...");

                delete net;
                continue;
            }

            if(m_networks.contains(net->m_id)) {
                qWarning("virt-env.jsonc: network %s already exist! Ignoring duplicate...", net->m_id.toUtf8().data());

                delete net;
                continue;
            }

            m_networks[net->m_id] = net;
        }
        catch(NetworkException &e) {
            qWarning("virt-env.jsonc: parsing network object failed: %s. Ignoring...", e.what());
        }
    }
}

void Presentation::parseVirtEnvJsonc() {
    if(!isFileValid("virt-env.jsonc"))
        return;

    QFile jsonFile(getFilePath("virt-env.jsonc"));

    if(jsonFile.open(QIODevice::ReadOnly) == false) {
        throw PresentationException("Failed to open virt-env.jsonc inside the archive");
    }
    
    QByteArray ba = jsonFile.readAll();
    json virtEnv;
    
    try {
        virtEnv = json::parse(ba.data(), nullptr, true, true);
        if(virtEnv.contains("virtualMachines"))
            parseVirtualMachines(virtEnv["virtualMachines"]);
        if(virtEnv.contains("networks"))
            parseNetworks(virtEnv["networks"]);
    }
    catch(json::exception &e) {
        QString exceptionStr = "Failed to parse \"virt-env.jsonc\": ";
        exceptionStr += e.what();
        throw PresentationException(exceptionStr);       
    }

    for(auto net : m_networks) {
        if(net->m_vmId != nullptr) {
            net->m_vm = getVirtualMachine(net->m_vmId);
            if(net->m_vm == nullptr) {
                QString exceptionStr = "virt-env.jsonc: net \"" + net->m_id + "\": ";
                exceptionStr += "Virtual machine \"" + net->m_vmId + "\" does not exist.";
                throw PresentationException(exceptionStr);
            }
        }
        else {
            net->m_vmId = QUuid::createUuid().toString();
            VirtualMachine* vm = new VirtualMachine(net->m_vmId, net, net->m_wan, "router", this);
            m_virtualMachines[vm->m_id] = vm;
        }
    }

    for(auto vm : m_virtualMachines) {
        Network* net = getNetwork(vm->m_netId);
        if(net) {
            vm->setNet(net);

            if(net->vm() == vm){
                vm->m_wan = net->hasWan();

                vm->start();
            }
        }
        else if(vm->m_netId != nullptr) {
            QString exceptionStr = "virt-env.jsonc: vm \"" + vm->m_id + "\": ";
            exceptionStr += "Network \"" + vm->m_netId + "\" does not exist.";
            throw PresentationException(exceptionStr);
        }
    }
    
}

Presentation::Presentation(QString path) {
    path = QFileInfo(path).absoluteFilePath();
    m_tmpDir.setAutoRemove(false);

    if(!m_tmpDir.isValid()) {
        throw PresentationException(m_tmpDir.errorString());
    }

    try {
        decompressArchive(path);
        parseVirtEnvJsonc();
        parseRootXml();
    }
    catch(PresentationException &e){
        m_tmpDir.remove();
        throw;
    }
}

Presentation::~Presentation() {
    m_tmpDir.remove();

    for(auto slide : m_slides)
        delete slide;
    
    for(auto vm : m_virtualMachines)
        vm->deleteLater();
    
    for(auto net : m_networks)
        net->deleteLater();
}

bool Presentation::isFileValid(QString path) {
    assert(m_tmpDir.isValid() == true);

    path = m_tmpDir.path() + "/" + path;
    QFileInfo fi(path);
    if(fi.exists() == false)
        return false;

    QString absoluteTmpDirPath = m_tmpDir.path();
    absoluteTmpDirPath = QFileInfo(absoluteTmpDirPath).absoluteFilePath();
    if(fi.absoluteFilePath().contains(absoluteTmpDirPath) == false)
        return false;

    return true;
}

QString Presentation::getFilePath(QString path) {
    if(isFileValid(path) == false)
        return nullptr;

    QFileInfo fi(m_tmpDir.path() + "/" + path);

    return fi.absoluteFilePath();
}