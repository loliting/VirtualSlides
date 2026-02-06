#include "VirtualMachine.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUuid>
#include <QtCore/QThread>
#include <QtWidgets/QMessageBox>
#include <QtCore/qprocessordetection.h>

#include "Application.hpp"
#include "Network.hpp"
#include "GuestBridge.hpp"

using namespace nlohmann;

static uint32_t cidCounter = 1000 * 1000;

#define KERNEL_DEFAULT_CMD "acpi=off reboot=t panic=-1 console=ttyS0 root=/dev/vda rw selinux=0 init=/sbin/vs_init "
#define KERNEL_QUIET_CMD "quiet " KERNEL_DEFAULT_CMD 
#define KERNEL_EARLYPRINTK_CMD "earlyprintk=ttyS0 " KERNEL_DEFAULT_CMD

InstallFile::InstallFile(nlohmann::json installFileObject, Presentation* pres){
    if(installFileObject.contains("content")) {
        std::string content = installFileObject["content"];
        this->content = std::vector<uint8_t>(content.begin(), content.end());
    }
    else if(installFileObject.contains("contentPath")) {
        QString contentPath = QString::fromStdString(installFileObject["contentPath"]); 
        QFile file = QFile(pres->getFilePath(contentPath));
        if(file.open(QIODevice::ReadOnly)) {
            QByteArray ba = file.readAll();
            content = std::vector<uint8_t>(ba.begin(), ba.end());
        }
        else
            throw VirtualMachineException("Failed to open installFile " + contentPath.toStdString());
    }
    else
        throw VirtualMachineException("installFile object is defined, but neither contentPath nor content strings exist");

    vmPath = QString::fromStdString(installFileObject["path"]); 

    if(installFileObject.contains("uid"))
        owner = installFileObject["uid"];

    if(installFileObject.contains("gid"))
        group = installFileObject["gid"];
    
    if(installFileObject.contains("perm")) {
        bool ok = false;
        perm = QString::fromStdString(installFileObject["perm"]).toUInt(&ok, 8);
        if(!ok) 
            throw VirtualMachineException("Failed to convert installFile's perm string to octal value");
        
        if(perm > 0777)
            throw VirtualMachineException("Invalid installFile's octal permissions");
    }
}

InitScript::InitScript(json initScriptObject, Presentation* pres){
    if(initScriptObject.contains("script")){
        std::string script = initScriptObject["script"];
        this->content = std::vector<uint8_t>(script.begin(), script.end());
    }
    else if(initScriptObject.contains("scriptPath")) {
        QString scriptPath = QString::fromStdString(initScriptObject["scriptPath"]); 
        QFile file = QFile(pres->getFilePath(scriptPath));
        if(file.open(QIODevice::ReadOnly)){
            QByteArray ba = file.readAll();
            content = std::vector<uint8_t>(ba.begin(), ba.end());
        }
        else
            throw VirtualMachineException("Failed to open script " + scriptPath.toStdString());
    }
    else
        throw VirtualMachineException("initScript object is defined, but neither scriptPath nor script strings exist");
}

json Subtask::toJson() {
    json json;

    json["id"] = id;
    json["done"] = done;

    return json;
}

FileSubtask::FileSubtask(nlohmann::json &taskObject) {
    content = taskObject["content"];
    path = taskObject["path"];
    
    if(taskObject.contains("id"))
        id = taskObject["id"];
    else
        id = QUuid::createUuid().toString().toStdString();
}

json FileSubtask::toJson() {
    json json = Subtask::toJson();

    json["content"] = content;
    json["path"] = path;
    json["type"] = "file";

    return json;
}

CommandSubtask::CommandSubtask(nlohmann::json &taskObject) {
    command = taskObject["command"];
    
    for(auto &arg : taskObject["arguments"])
        args.push_back(arg);
    
    if(taskObject.contains("exitCode"))
        exitCode = taskObject["exitCode"];
    else
        exitCode = 0;

    if(taskObject.contains("id"))
        id = taskObject["id"];
    else
        id = QUuid::createUuid().toString().toStdString();
}

json CommandSubtask::toJson() {
    json json = Subtask::toJson();

    json["command"] = command;
    json["args"] = args;
    json["exitCode"] = exitCode;
    json["type"] = "command";

    return json;
}

Task::Task(json &taskObject) {
    if(taskObject.contains("description"))
        description = QString::fromStdString(taskObject["description"]);

    for(auto &fileTaskObj : taskObject["files"]) {
        FileSubtask *subtask = new FileSubtask(fileTaskObj);
        if(subtask->id.empty()){
            qWarning("virt-env.jsonc: Found file subtask object with empty ID string. Ignoring...");

            delete subtask;
            continue;
        }
        if(subtasks.contains(subtask->id)){
            qWarning("virt-env.jsonc: File subtask %s already exist! Ignoring duplicate...", subtask->id.c_str());
            
            delete subtask;
            continue;
        }
        subtasks[subtask->id] = subtask;
    }

    for(auto &commandTaskObj : taskObject["commands"]) {
        CommandSubtask *subtask = new CommandSubtask(commandTaskObj);
        if(subtask->id.empty()){
            qWarning("virt-env.jsonc: Found command subtask object with empty ID string. Ignoring...");
            
            delete subtask;
            continue;
        }
        if(subtasks.contains(subtask->id)){
            qWarning("virt-env.jsonc: Command subtask %s already exist! Ignoring duplicate...", subtask->id.c_str());
            
            delete subtask;
            continue;
        }
        subtasks[subtask->id] = subtask;
    }

    for(auto &taskPath : taskObject["taskPaths"]) {
        QList<Subtask*> path;
        for(auto &task : taskPath)
            path += subtasks[task];
        if(!path.isEmpty())
            taskPaths += path;
    }

    if(taskPaths.isEmpty())
        taskPaths += QList(subtasks.begin(), subtasks.end());
    
    id = QUuid::createUuid().toString().toStdString();
}

json Task::toJson() {
    json taskJson;

    taskJson["id"] = id;

    std::vector<json> subtasksJson;
    for(auto &subtask : subtasks)
        subtasksJson.push_back(subtask->toJson());

    taskJson["subtasks"] = subtasksJson;


    std::vector<std::vector<std::string>> pathsJson;
    for(auto &path : taskPaths){
        std::vector<std::string> pathJson;
        for(auto &subtask : path)
            pathJson.push_back(subtask->id);
        
        pathsJson.push_back(pathJson);
    }

    taskJson["paths"] = pathsJson;

    return taskJson;
}

Task::~Task() {
    for(auto subtask : subtasks)
        delete subtask;
    
    subtasks.clear();
    taskPaths.clear();
}

VirtualMachine::VirtualMachine(json &vmObject, Presentation* pres) : m_presentation(pres), m_cid(cidCounter++) {
    m_id = QString::fromStdString(vmObject["id"]);
    m_image = QString::fromStdString(vmObject["image"]);
    
    if(vmObject.contains("netId"))
        m_netId = QString::fromStdString(vmObject["netId"]);
    else
        m_netId = nullptr;

    if(vmObject.contains("hostname"))
        m_hostname = QString::fromStdString(vmObject["hostname"]);
    else
        m_hostname = m_id;
    
    for(auto installFileObj : vmObject["installFiles"])
        m_installFiles += InstallFile(installFileObj, pres);
    
    for(auto initScriptObj : vmObject["initScripts"])
        m_initScripts += InitScript(initScriptObj, pres);
    
    for(auto taskObj : vmObject["tasks"]){
        Task* task = new Task(taskObj); 
        m_tasks[task->id] = task;
    }
    
    m_guestBridge = new GuestBridge(this);
    m_guestBridge->start();
    
    createImageFile();
}

VirtualMachine::VirtualMachine(QString id, Network* net, bool hasWan, QString image, Presentation* pres)
    : m_presentation(pres), m_cid(cidCounter++), m_id(id), m_net(net),
    m_netId(net->id()), m_wan(hasWan), m_image(image),
    m_macAddress(m_net->generateNewMacAddress()), m_hostname(m_id),
    m_guestBridge(new GuestBridge(this))
{
    m_guestBridge->start();

    createImageFile();
}

void VirtualMachine::createImageFile(){
    m_diskImage = Config::getDiskImage(m_image);
    if(m_diskImage == nullptr){
        QString exceptionStr = "Could not create vm \"" + m_id + "\": image \"" + m_image + "\" does not exist";
        throw VirtualMachineException(exceptionStr.toStdString());
    }

    if(m_imageFile.open() == false){
        QString exceptionStr = "Could not create temporary file: " + m_imageFile.errorString();
        throw VirtualMachineException(exceptionStr.toStdString());
    }

    QFile orginalDiskFile(m_diskImage->path);
    if(orginalDiskFile.open(QIODevice::ReadOnly) == false){
        QString exceptionStr = "Could not open disk image \"" + m_diskImage->path + "\": ";
        exceptionStr += orginalDiskFile.errorString();
        throw VirtualMachineException(exceptionStr.toStdString());
    }
    
    m_imageFile.write(orginalDiskFile.readAll());
    orginalDiskFile.close();
    m_imageFile.close();
}

QStringList VirtualMachine::getArgs(){
    QStringList ret;
    ret << "-machine" << "microvm,acpi=off";
    if(Config::getKvmEnabled())
        ret << "-enable-kvm" << "-cpu" << "host";

    ret << "-smp" << QString::number(Config::getGuestProcCount())
        << "-m" << QString::number(Config::getGuestMemSize()) + "M"
        << "-mem-prealloc" << "-no-reboot"
        << "-kernel" << Config::getGuestKernelPath()
        << "-append" << KERNEL_QUIET_CMD + m_diskImage->initSysPath
        << "-nodefaults" << "-no-user-config" << "-nographic"
        << "-device" << "virtio-serial-device"
        << "-chardev" << "socket,id=char0,path=" + m_consoleServer->fullServerName()
        << "-serial" << "chardev:char0"
        << "-drive" << "id=root,file=" + m_imageFile.fileName() + ",format=qcow2,if=none"
        << "-device" << "virtio-blk-device,drive=root"
        << "-device" << "vhost-vsock-device,guest-cid=" + QString::number(m_cid);
    if(m_net && !m_macAddress.isEmpty())
        ret << "-netdev" << "socket,id=eth0,localaddr=127.0.0.1,mcast=" VNET_MCAST_ADDR ":" + QString::number(m_net->mcastPort())
            << "-device" << "virtio-net-device,netdev=eth0,mac=" + m_macAddress;
    if(m_wan)
        ret << "-netdev" << "user,id=eth1,net=100.127.254.0/24,dhcpstart=100.127.254.8"
            << "-device" << "virtio-net-device,netdev=eth1,mac=00:00:00:00:00:01";
    
    return ret;
}

void VirtualMachine::setNet(Network* net){
    m_net = net;
    if(net){
        m_macAddress = net->generateNewMacAddress();
    }
    if(m_isRunning){
        restart();
    }
    emit networkChanged();
}

void VirtualMachine::handleNewConsoleSocketConnection() {
    UnixSocket* conn = m_consoleServer->nextPendingConnection();
    if(m_consoleSocket){
        m_terminalSockets.append(conn);
        connect(conn, &UnixSocket::readyRead, this, [this, conn]{ handleClientConsoleSockReadReady(conn); });
    }
    else{
        m_consoleSocket = conn;
        connect(m_consoleSocket, SIGNAL(readyRead()), this, SLOT(handleConsoleSockReadReady()));
        emit vmStarted();
    }
}

void VirtualMachine::start() {
    if(m_isRunning)
        return;

    if(m_guestBridge && !m_guestBridge->isListening())
        m_guestBridge->start();
    
    m_consoleServer = new UnixSocketServer();
    QString serverNameUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!m_consoleServer->listen(serverNameUuid)) {
        QMessageBox::critical(qApp->activeWindow(),
            "Fatal error",
            "Failed to start VM: " + m_consoleServer->errorString()
            + "(" + m_consoleServer->fullServerName() + ")"
        );
        return;
    }
    m_serverName = m_consoleServer->fullServerName();
    
    connect(m_consoleServer, SIGNAL(newConnection()),
        this, SLOT(handleNewConsoleSocketConnection(void)));

    m_vmProcess = new QProcess();

    m_vmProcess->setProgram("qemu-system-x86_64");
    m_vmProcess->setArguments(getArgs());
    m_vmProcess->start();

    connect(m_vmProcess, &QProcess::finished, this, &VirtualMachine::handleVmProcessFinished);
    connect(m_vmProcess, &QProcess::started, this, [this]{
        if(m_guestBridge && !m_guestBridge->isListening())
            m_guestBridge->start();
    });

    m_isRunning = true;
}

void VirtualMachine::handleVmProcessFinished(int exitCode) {
    if(m_consoleSocket){
        m_consoleSocket->disconnect();
        m_consoleSocket->deleteLater();
    }
    m_consoleSocket = nullptr;
    

    for (auto termSock : m_terminalSockets) {
        termSock->close();
        termSock->deleteLater();
    }
    m_terminalSockets.clear();

    if(m_consoleServer){
        m_consoleServer->close();
        m_consoleServer->deleteLater();
    }
    m_consoleServer = nullptr;


    m_isRunning = false;

    m_vmProcess->deleteLater();
    m_vmProcess = nullptr;

    emit vmStopped();
    if(m_shouldRestart) {
        m_shouldRestart = false;
        start();
    }

    /*
     * If the guest bridge is not listing and qemu process finished, 
     * it probably that something crashed
     */
    if(m_retryCounter < 3) {
        if(m_guestBridge && !m_guestBridge->isListening()) {
            m_guestBridge->start();
            start();
            m_retryCounter += 1;
        }
        else
            m_retryCounter = 0;
    }
    else if(exitCode != 0){
        QMessageBox(QMessageBox::Critical,
            "Virtual Slides: VM error",
            "QEMU failed: " + m_vmProcess->readAllStandardError(),
            QMessageBox::Ok,
            qApp->activeWindow()
        ).exec();
    }
}

void VirtualMachine::stop() {
    if(!m_isRunning || !m_vmProcess)
        return;

    if(m_vmProcess->state() == QProcess::Running){
        m_vmProcess->terminate();
    }
}

void VirtualMachine::restart() {
    m_shouldRestart = true;
    stop();
}

void VirtualMachine::handleClientConsoleSockReadReady(UnixSocket* sock) {
    m_consoleSocket->write(sock->readAll());
}

void VirtualMachine::handleConsoleSockReadReady() {
    QByteArray data = m_consoleSocket->readAll();

    for (auto term : m_terminalSockets) {
        term->write(data);
    }
}

VirtualMachine::~VirtualMachine() {
    stop();
    m_imageFile.close();
    if(m_guestBridge){
        m_guestBridge->stop();
        m_guestBridge->deleteLater();
    }
    m_guestBridge = nullptr;

    for(auto task : m_tasks)
        delete task;

    m_tasks.clear();
}

void VirtualMachine::registerWidget(VirtualMachineWidget* w, QSize size) {
    m_widgetSizes[w] = size;

    int minW = INT_MAX, minH = INT_MAX;

    for(auto &size : m_widgetSizes) {
        if(size.width() < minW)
            minW = size.width();
        if(size.height() < minH)
            minH  = size.height();
    }

    m_minimumWidgetSize = QSize(minW, minH);
}