#ifndef PRESENTATION_HPP
#define PRESENTATION_HPP

#include <QtCore/QObject>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtCore/QTemporaryDir>

#include <exception>

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

    Q_PROPERTY(QWidget* widget READ widget WRITE setWidget NOTIFY widgetSet)
   
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY positionChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY positionChanged)
    Q_PROPERTY(QPointF pos READ pos WRITE setPos NOTIFY positionChanged)

    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY sizeChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY sizeChanged)
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged)
public:
    PresentationElement();
    PresentationElement(qreal x, qreal y, qreal width, qreal height);

    QWidget* widget() const { return m_widget; }
    void setWidget(QWidget* w);

    virtual void setX(qreal x) { m_pos.setX(x); }
    virtual void setY(qreal y) { m_pos.setY(y); }
    virtual void setPos(QPointF pos) { m_pos = pos; }

    virtual void setWidth(qreal width) { m_size.setWidth(width); }
    virtual void setHeight(qreal height) { m_size.setHeight(height); }
    virtual void setSize(QSizeF size) { m_size = size; }

    virtual qreal x() const { return m_pos.x(); }
    virtual qreal y() const  { return m_pos.y(); }
    virtual QPointF pos() const { return m_pos; }

    virtual qreal width() const { return m_size.width(); }
    virtual qreal height() const { return m_size.height(); }
    virtual QSizeF size() const { return m_size; }
signals:
    void positionChanged();
    void sizeChanged();
    void widgetSet();
private slots:
    virtual void recalculateRealDim();
private:
    QSizeF m_size = QSizeF();
    QPointF m_pos = QPointF();
    QWidget* m_widget = Q_NULLPTR;
};

class PresentationTextBoxElement : public PresentationElement
{
    Q_OBJECT
public:
    PresentationTextBoxElement(QWidget* parent, QString text);
    PresentationTextBoxElement(QWidget* parent, qreal x, qreal y, qreal width, qreal height, QString text);
    ~PresentationTextBoxElement();
private:
    QLabel* m_widget;
};


class PresentationSlide : public QLabel 
{
    Q_OBJECT
public:
    PresentationSlide(QString bg = QString());
    ~PresentationSlide();
signals:
    void resize(int w, int h);
private:
    QString m_bg;
    QList<PresentationElement*> m_elements;
    
protected:
    void resizeEvent(QResizeEvent *event);

    friend class Presentation;
};


struct Presentation{
public:
    Presentation(QString path);
    ~Presentation();
private:
    void decompressVslidesArchive(QString path);
    void parseXml();
public:
    QString m_title;
    QList<PresentationSlide*> m_slides;
private:
    QTemporaryDir m_tmpDir = QTemporaryDir();
};

#endif // PRESENTATION_HPP