#ifndef PRESENTATION_HPP
#define PRESENTATION_HPP

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

struct Presentation{
public:
    Presentation(QString path);
    ~Presentation();
private:
    void decompressVslidesArchive(QString path);
private:
    QTemporaryDir m_tmpDir = QTemporaryDir();
};

#endif // PRESENTATION_HPP