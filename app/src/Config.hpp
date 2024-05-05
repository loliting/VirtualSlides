#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtWidgets/QApplication>

#include <exception>

class ConfigException : std::exception
{
public:
    ConfigException(QString cause) { m_what = cause.toStdString(); }
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

class Config
{
public:
    static void Initializate(QString configJsonPath = QApplication::applicationDirPath() + "/Config.jsonc");

    static void CleanUp();
    static DiskImage* getDiskImage(QString name);
    static size_t getGuestMemSize();
    static size_t getGuestProcCount();
    static QString getGuestKernelPath();
    static bool getKvmEnabled();
private:
    static bool m_initializated;
    static QMap<QString, DiskImage*> m_diskImages;

    static size_t m_guestMemSize;
    static size_t m_guestProcCount;
    static QString m_guestKernelPath;
    static bool m_kvmEnabled;
};

#endif // CONFIG_HPP