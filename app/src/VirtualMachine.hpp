#ifndef VIRTUALMACHINE_HPP
#define VIRTUALMACHINE_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QTemporaryFile>

#include <qtermwidget5/qtermwidget.h>

#include <exception>

#include "third-party/RapidXml/rapidxml.hpp"

class VirtualMachine;

#include "Network.hpp"

class VirtualMachineException : public std::exception
{
public:
    VirtualMachineException(QString cause) { m_what = cause.toStdString(); }
    QString cause() const throw() { return QString::fromStdString(m_what); }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};

struct InstallFile
{
    InstallFile(rapidxml::xml_node<char>* installFileNode);
    QString vmPath;

    QString hostPath;
    QString content;
};

struct FileObjective
{
    FileObjective(rapidxml::xml_node<char>* fileNode);
 
    QString vmPath;

    QStringList matchStrings;
    bool caseSensitive = false;
    bool trimWhitespace = true;
    bool normalizeWhitespace = true;

    QString regexMatch;
};

struct CommandObjective
{
    CommandObjective(rapidxml::xml_node<char>* runCommandNode);
 
    /* Each list entry is alternative command (eg. for different editors)*/
    QStringList command;
    int expectedExitCode = 0;
    /* Each list entry is a list of alternative arguments */
    QList<QStringList> args;
    QStringList fileArgs;
};

class VirtualMachine : QObject
{
    Q_OBJECT
public:
    QTermWidget* widget() const { return m_widget; }
private:
    VirtualMachine(rapidxml::xml_node<char>* vmNode);
    VirtualMachine(QString id, Network* net, bool hasSlirpNetDev, bool dhcpServer, QString image);

    void createWidget();
    void createImageFile();
    QStringList getArgs();

    QString m_id;
    QString m_netId;
    Network* m_net = nullptr;

    bool m_hasSlirpNetDev = false;

    QString m_image;

    QString m_hostname;
    bool m_dhcpClient = true;
    bool m_dhcpServer = false;
    QList<InstallFile> m_installFiles;

    QList<FileObjective> m_fileObjectives;
    QList<CommandObjective> m_commandObjectives;

    QTermWidget* m_widget = nullptr;
    QTemporaryFile m_imageFile;

    friend class VirtualMachineManager;
};

#endif // VIRTUALMACHINE_HPP