#ifndef VIRTUALMACHINE_HPP
#define VIRTUALMACHINE_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QTemporaryFile>
#include <QtCore/QProcess>
#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QLocalServer>

#include <exception>

#include "third-party/RapidXml/rapidxml.hpp"

class VirtualMachine;

#include "Network.hpp"
#include "VirtualMachineWidget.hpp"
#include "Config.hpp"
#include "GuestBridge.hpp"

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

class VirtualMachine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Network* net READ net WRITE setNet NOTIFY networkChanged);
public:
    QString id() const { return m_id; }
    Network* net() const { return m_net; }
    uint32_t cid() const { return m_cid; }
    QString serverName() const { return m_serverName; }
    void setNet(Network* net);
    ~VirtualMachine();
private:
    VirtualMachine(rapidxml::xml_node<char>* vmNode);
    VirtualMachine(QString id, Network* net, bool hasSlirpNetDev, QString image);

    void createImageFile();
    QString m_id;
    QString m_netId;
    Network* m_net = nullptr;
    QString m_macAddress;

    bool m_hasSlirpNetDev = false;

    QString m_image;
    DiskImage* m_diskImage;

    QString m_hostname;
    QString m_motd;
    
    QList<InstallFile> m_installFiles;

    QList<FileObjective> m_fileObjectives;
    QList<CommandObjective> m_commandObjectives;

    QTemporaryFile m_imageFile;


    QStringList getArgs();
    bool m_isRunning = false;
    bool m_shouldRestart = false; /* Should vm restart instead of stopping on stop() */
    QProcess* m_vmProcess;

    QString m_serverName;
    QLocalServer* m_consoleServer = nullptr;
    QLocalSocket* m_consoleSocket = nullptr;
    QList<QLocalSocket*> m_terminalSockets;

    GuestBridge* m_guestBridge = nullptr;
    uint32_t m_cid;
signals:
    void networkChanged();
    void vmStarted();
    void vmStopped();

private slots:
    void handleNewConsoleSocketConnection();
    void handleConsoleSockReadReady();
    void handleClientConsoleSockReadReady(QLocalSocket* sock);
    
    void handleVmProcessFinished(int);
public slots:
    void start();
    void stop();
    void restart();

    friend class VirtualMachineManager;
    friend class GuestBridge;
};

#endif // VIRTUALMACHINE_HPP