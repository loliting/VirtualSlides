#ifndef PRESENTATION_HPP
#define PRESENTATION_HPP

#include <QtCore/QObject>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtCore/QTemporaryDir>

#include <exception>

#include "VirtualMachineManager.hpp"
#include "NetworkManager.hpp"

class PresentationException : public std::exception
{
public:
    PresentationException(QString cause) { m_what = cause.toStdString(); }
    QString cause() const throw() { return QString::fromStdString(m_what); }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};


class PresentationElement : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QWidget* widget READ widget WRITE setWidget NOTIFY widgetSet);
   
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY positionChanged);
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY positionChanged);
    Q_PROPERTY(QPointF pos READ pos WRITE setPos NOTIFY positionChanged);

    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY sizeChanged);
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY sizeChanged);
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged);
public:
    PresentationElement();
    PresentationElement(qreal x, qreal y, qreal width, qreal height, QWidget *w);

    QWidget* widget() const { return m_widget; }
    void setWidget(QWidget* w);

    void setX(qreal x) { m_pos.setX(x); }
    void setY(qreal y) { m_pos.setY(y); }
    void setPos(QPointF pos) { m_pos = pos; }

    void setWidth(qreal width) { m_size.setWidth(width); }
    void setHeight(qreal height) { m_size.setHeight(height); }
    void setSize(QSizeF size) { m_size = size; }

    qreal x() const { return m_pos.x(); }
    qreal y() const  { return m_pos.y(); }
    QPointF pos() const { return m_pos; }

    qreal width() const { return m_size.width(); }
    qreal height() const { return m_size.height(); }
    QSizeF size() const { return m_size; }
signals:
    void positionChanged();
    void sizeChanged();
    void widgetSet();
private slots:
    void recalculateRealDim();
private:
    QSizeF m_size = QSizeF();
    QPointF m_pos = QPointF();
    QWidget* m_widget = nullptr;
};

class PresentationSlide : public QLabel 
{
    Q_OBJECT
public:
    PresentationSlide(QColor bg = QColor("white"));
    PresentationSlide(QPixmap bg);
    ~PresentationSlide();
signals:
    void resize(int w, int h);
private:
    QList<PresentationElement*> m_elements;
    
protected:
    void resizeEvent(QResizeEvent *event) override;

    friend class Presentation;
};


struct Presentation {
public:
    Presentation(QString path);
    ~Presentation();
    bool isFileValid(QString path);
    QString getFilePath(QString path);
private:
    void decompressArchive(QString path);
    void parseRootXml();
public:
    QString m_title;
    QList<PresentationSlide*> m_slides;
private:
    QTemporaryDir m_tmpDir;
    VirtualMachineManager* m_vmManager = nullptr;
    NetworkManager* m_netManager = nullptr;
};

#endif // PRESENTATION_HPP