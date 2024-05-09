#ifndef VIRTUALMACHINE_HPP
#define VIRTUALMACHINE_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QTemporaryFile>
#include <QtCore/QProcess>
#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QLocalServer>
#include <QtCore/QUuid>

#include <exception>

#include "third-party/nlohmann/json.hpp"


#include "Config.hpp"

class Network;
class GuestBridge;
class Presentation;

class VirtualMachineException : public std::exception
{
public:
    VirtualMachineException(std::string what) { m_what = what; }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};

struct InstallFile
{
    InstallFile(nlohmann::json installFileObject, Presentation* pres);
    QString vmPath;

    std::vector<uint8_t> content;

    mode_t perm = 0640;
    uid_t owner = 0;
    gid_t group = 0;
};

struct InitScript
{
    InitScript(nlohmann::json initScriptObject, Presentation* pres);
    
    std::vector<uint8_t> content;
};

struct Subtask
{
    virtual nlohmann::json toJson();


    std::string id = std::string();

    bool done = false;
};

struct CommandSubtask : public Subtask
{
    CommandSubtask(nlohmann::json &taskObject);
    
    nlohmann::json toJson() override;


    std::string command = std::string();
    std::vector<std::string> args = std::vector<std::string>();

    int exitCode = 0;
};

struct FileSubtask : public Subtask
{
    FileSubtask(nlohmann::json &taskObject);

    nlohmann::json toJson() override;


    std::string path = std::string();
    std::string content = std::string();
};

struct Task
{
    Task(nlohmann::json &taskObject);
    ~Task();
    
    nlohmann::json toJson();

    std::string id;

    QString description;

    QMap<std::string, Subtask*> subtasks;

    QList<QList<Subtask*>> taskPaths;
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
    VirtualMachine(nlohmann::json &vmObject, Presentation* pres);
    VirtualMachine(QString id, Network* net, bool wan, QString image, Presentation* pres);

    void createImageFile();
    QString m_id;
    QString m_netId;
    Network* m_net = nullptr;
    QString m_macAddress;

    bool m_wan = false;

    QString m_image;
    DiskImage* m_diskImage;

    QString m_hostname;
    
    QList<InstallFile> m_installFiles;
    QList<InitScript> m_initScripts;

    QList<Task*> m_tasks;

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

    Presentation* m_presentation;
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

    friend class Presentation;
    friend class GuestBridge;
    friend class VirtualMachineWidget;
};

#endif // VIRTUALMACHINE_HPP