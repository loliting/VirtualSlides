#include "Presentation.hpp"

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
    qreal x = 0, y = 0, w = 0, h = 0;
    
    xml_attribute<char>* attrib = nullptr;
    
    attrib = xmlNode->first_attribute("x", 0UL, false);
    if(attrib != nullptr){
        x = QString(attrib->value()).toDouble() / 100;
    }
    attrib = xmlNode->first_attribute("y", 0UL, false);
    if(attrib != nullptr){
        y = QString(attrib->value()).toDouble() / 100;
    }
    attrib = xmlNode->first_attribute("width", 0UL, false);
    if(attrib != nullptr){
        w = QString(attrib->value()).toDouble() / 100;
    }
    attrib = xmlNode->first_attribute("height", 0UL, false);
    if(attrib != nullptr){
        h = QString(attrib->value()).toDouble() / 100;
    }
    e->setX(x);
    e->setY(y);
    e->setWidth(w);
    e->setHeight(h);
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

PresentationElement::PresentationElement(qreal x, qreal y, qreal height, qreal width) {
    PresentationElement();
    m_pos = QPointF(x, y);
    m_size = QSizeF(width, height);
}

void PresentationElement::setWidget(QWidget* w) {
    assert(m_widget == nullptr);
    m_widget = w;

    QWidget* parent = m_widget->parentWidget();
    assert(parent != nullptr);

    connect(parent, SIGNAL(resize(int, int)), this, SLOT(recalculateRealDim(void)));
}

PresentationTextBoxElement::PresentationTextBoxElement(QWidget* parent, QString text)
    : PresentationElement()
{
    m_widget = new QLabel(parent);
    setWidget(m_widget);

    m_widget->setTextFormat(Qt::RichText);
    m_widget->setText(text);
    m_widget->show();
}

PresentationTextBoxElement::PresentationTextBoxElement(QWidget* parent, qreal x, qreal y, qreal height, qreal width, QString text)
    : PresentationElement(x, y, width, height)
{
    PresentationTextBoxElement(parent, text);
}

PresentationTextBoxElement::~PresentationTextBoxElement() {
    delete m_widget;
}

PresentationSlide::PresentationSlide(QString bg) {
    m_bg = bg;
    setAutoFillBackground(true);
    setPalette(QPalette(QColor(m_bg)));
}

PresentationSlide::~PresentationSlide() {
    for(auto element : m_elements){
        delete element;
    }
}

void PresentationSlide::resizeEvent(QResizeEvent *event) {
    emit resize(event->size().width(), event->size().height());
    QLabel::resizeEvent(event);
}

void Presentation::decompressVslidesArchive(QString path) {
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

    QDir cwd = QDir::current();

    size_t zEntriesCount = zip_get_num_entries(zipArchive, 0);
    for(size_t i = 0; i < zEntriesCount; ++i){
        if(zip_stat_index(zipArchive, i, 0, &zStat) == -1){
            zip_error_t* error = zip_get_error(zipArchive);
            QString exceptionString = "Failed to decompress '" + path + "': ";
                + zip_error_strerror(error);
            zip_close(zipArchive);
            throw PresentationException(exceptionString);
        }
        QString fileName = QString(zStat.name);
        if(fileName[fileName.length() - 1] == '/'){
            if(!cwd.mkpath(fileName)){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }
        }
        else{
            QFile file(fileName);
            zf = zip_fopen_index(zipArchive, i, 0);
            if(file.open(QIODevice::WriteOnly) == false || zf == nullptr){
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

void Presentation::parseXml() {
    QFile rootXml("root.xml");

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
    
    xml_node<char>* textBoxNode;
    xml_node<char>* textNode;

    if(slideNode == nullptr){
        QString exceptionStr = "Failed to parse root.xml: ";
        exceptionStr += "\"<Presentation>\" node have to contain at least one \"<Slide>\" node";
        throw PresentationException(exceptionStr);
    }
    while(slideNode){
        PresentationSlide* slide = nullptr;

        bgAttribute = slideNode->first_attribute("bg", 0UL, false);
        if(bgAttribute){
            slide = new PresentationSlide(bgAttribute->value());
        }
        else{
            slide = new PresentationSlide();
        }
        
        textBoxNode = slideNode->first_node("TextBox", 0UL, false);
        while(textBoxNode){
            textNode = textBoxNode->first_node("Text", 0UL, false);
            std::stringstream textNodeSs;
            textNodeSs << *textNode;
            std::string text = textNodeSs.str();
            text = text.substr(TEXT_NODE_SZ, text.length() - TEXT_NODE_SZ * 2);

            PresentationTextBoxElement* tB = nullptr;
            tB = new PresentationTextBoxElement(slide, QString::fromStdString(text));
            parseXmlDimensions(textBoxNode, tB);

            slide->m_elements.append(tB);
            textBoxNode = textBoxNode->next_sibling("TextBox", 0UL, false);
        }

        m_slides.append(slide);
        slideNode = slideNode->next_sibling("Slide", 0UL, false);
    }

    rootXml.close();
}

Presentation::Presentation(QString path) {
    path = QFileInfo(path).absoluteFilePath();
    m_tmpDir.setAutoRemove(false);

    QString oldCwd = QDir::currentPath();
    if(!QDir::setCurrent(m_tmpDir.path())){
        throw PresentationException("Failed to chdir into tmp dir");
    }

    if(!m_tmpDir.isValid()){
        throw PresentationException(m_tmpDir.errorString());
    }

    try{
        decompressVslidesArchive(path);
        parseXml();
    }
    catch(PresentationException &e){
        QDir::setCurrent(oldCwd);
        m_tmpDir.remove();
        throw;
    }
    QDir::setCurrent(oldCwd);
}

Presentation::~Presentation() {
    m_tmpDir.remove();
    for(auto slide : m_slides){
        delete slide;
    }
}