#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

#include <exception>

class DiskImageManagerException : std::exception
{
public:
    DiskImageManagerException(QString cause) { m_what = cause.toStdString(); }
    QString cause() const throw() { return QString::fromStdString(m_what); }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};

struct DiskImage
{
    QString name;
    QString initSysPath;
    QString path;
};

class DiskImageManager
{
public:
    static void Initializate();
    static void CleanUp();
    static DiskImage* getDiskImage(QString name);
private:
    static bool m_initializated;
    static QMap<QString, DiskImage*> m_diskImages;
};