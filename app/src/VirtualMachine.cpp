#include "VirtualMachine.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUuid>
#include <QtCore/QThread>
#include <QtWidgets/QMessageBox>
#include <QtCore/qprocessordetection.h>

#include "Application.hpp"

using namespace rapidxml;

static uint32_t cidCounter = 1000 * 1000;

#define KERNEL_DEFAULT_CMD "acpi=off reboot=t panic=-1 console=ttyS0 root=/dev/vda rw selinux=0 init=/sbin/vs_init "
#define KERNEL_QUIET_CMD "quiet " KERNEL_DEFAULT_CMD 
#define KERNEL_EARLYPRINTK_CMD "earlyprintk=ttyS0 " KERNEL_DEFAULT_CMD

static bool getBool(QString boolStr, bool defaultValue){
    QString lowerBoolStr = boolStr.toLower();

    if(lowerBoolStr == "true"){
        return true;
    }
    else if(lowerBoolStr == "false"){
        return false;
    }
    return defaultValue;
}

static bool getXmlBoolAttrib(xml_node<char>* node, const char* name, bool defaultValue){
    if(node == nullptr){
        return defaultValue;
    }

    xml_attribute<char>* attrib = node->first_attribute(name, 0UL, false);
    if(attrib == nullptr){
        return defaultValue;
    }

    return getBool(attrib->value(), defaultValue);
}

static bool getXmlBoolValue(xml_node<char>* parentNode, const char* name, bool defaultValue){
    if(parentNode == nullptr){
        return defaultValue;
    }

    xml_node<char>* node = parentNode->first_node(name, 0UL, false);
    if(node == nullptr){
        return defaultValue;
    }

    return getBool(node->value(), defaultValue);
}

InstallFile::InstallFile(xml_node<char>* installFileNode, Presentation* pres){
    xml_node<char>* pathNode = installFileNode->first_node("path", 0UL, false);
    vmPath = pathNode->value();
    
    xml_node<char>* contentNode = installFileNode->first_node("content", 0UL, false);
    xml_attribute<char>* contentPathAttrib = nullptr;

    xml_attribute<char>* ownerAttrib = installFileNode->first_attribute("owner", 0UL, false);
    xml_attribute<char>* groupAttrib = installFileNode->first_attribute("group", 0UL, false);
    xml_attribute<char>* permAttrib = installFileNode->first_attribute("perm", 0UL, false);

    if(contentNode){
        content = contentNode->value();
        contentPathAttrib = contentNode->first_attribute("path", 0UL, false);
        if(contentPathAttrib){
            if(pres->isFileValid(contentPathAttrib->value())){
                QFile file = QFile(pres->getFilePath(contentPathAttrib->value()));
                if(file.open(QIODevice::ReadOnly))
                    content = file.readAll();
                else
                    qWarning("vms.xml: Failed to open %s.", contentPathAttrib->value());
            }
            else
                qWarning("vms.xml: install-file's content path argument exist, but it's not valid.");
        }
    }

    if(ownerAttrib) {
        bool ok = false;
        owner = QString(ownerAttrib->value()).toUInt(&ok);
        if(!ok){
            owner = 0;
            qWarning("vms.xml: owner attribute value is not valid (%s)", ownerAttrib->value());
        }
    }

    if(groupAttrib) {
        bool ok = false;
        group = QString(groupAttrib->value()).toUInt(&ok);
        if(!ok){
            group = 0;
            qWarning("vms.xml: group attribute value is not valid (%s)", groupAttrib->value());
        }
    }
    
    if(permAttrib) {
        bool ok = false;
        perm = QString(permAttrib->value()).toUInt(&ok, 8);
        if(!ok || perm > 0777){
            perm = 0;
            qWarning("vms.xml: perm attribute value is not valid (%s)", permAttrib->value());
        }
    }
}

FileObjective::FileObjective(xml_node<char>* fileNode){
    xml_node<char>* pathNode = fileNode->first_node("path", 0UL, false);
    if(pathNode != nullptr){
        vmPath = pathNode->value();
    }

    caseSensitive = getXmlBoolAttrib(fileNode, "case-sensitive", false);
    trimWhitespace = getXmlBoolAttrib(fileNode, "trim-whitespace", true);
    normalizeWhitespace = getXmlBoolAttrib(fileNode, "normalize-whitespace", true);
    
    xml_node<char>* regexMatchNode = fileNode->first_node("regex-match", 0UL, false);
    if(regexMatchNode != nullptr){
        xml_attribute<char>* regexAttrib = regexMatchNode->first_attribute("regex", 0UL, false);
        if(regexAttrib != nullptr){
            regexMatch = regexAttrib->value();
        }
    }

    xml_node<char>* matchNode = fileNode->first_node("match", 0UL, false);
    while(matchNode){
        xml_attribute<char>* stringAttrib = matchNode->first_attribute("string", 0UL, false);
        if(stringAttrib != nullptr){
            matchStrings.append(stringAttrib->value());
        }

        matchNode = matchNode->next_sibling("match", 0UL, false);
    }
}

CommandObjective::CommandObjective(xml_node<char>* runCommandNode){
    xml_node<char>* commandNode = runCommandNode->first_node("command", 0UL, false);
    if(commandNode){
        xml_attribute<char>* altAttrib = commandNode->first_attribute("alt", 0UL, false);
        if(altAttrib != nullptr){
            command.append(QString(altAttrib->value()).split(";", Qt::SkipEmptyParts));
        }
        if(commandNode->value()){
            command.append(commandNode->value());
        }
        xml_attribute<char>* exitCodeAttrib = commandNode->first_attribute("exit-code", 0UL, false);
        if(exitCodeAttrib != nullptr){
            bool ok = false;
            expectedExitCode = QString(exitCodeAttrib->value()).toInt(&ok);
            if(ok == false){
                qWarning() << "Failed to convert" << exitCodeAttrib->value() << "into integer value";
                expectedExitCode = 0;
            }
        }
    }

    xml_node<char>* argNode = runCommandNode->first_node("arg", 0UL, false);
    while(argNode){
        QStringList arg;

        xml_attribute<char>* altAttrib = argNode->first_attribute("alt", 0UL, false);
        if(altAttrib != nullptr){
            arg.append(QString(altAttrib->value()).split(";", Qt::SkipEmptyParts));
        }
        if(argNode->value()){
            arg.append(argNode->value());
        }
        args.append(arg);
        
        xml_attribute<char>* fileAttrib = argNode->first_attribute("file", 0UL, false);
        if(fileAttrib != nullptr){
            fileArgs.append(fileAttrib->value());
        }

        argNode = argNode->next_sibling("arg", 0UL, false);
    }
}

VirtualMachine::VirtualMachine(xml_node<char>* vmNode, Presentation* pres) : m_presentation(pres) {
    xml_attribute<char>* idAttrib = vmNode->first_attribute("id", 0UL, false);
    if(idAttrib == nullptr){
        throw VirtualMachineException("<VM> node does not contain id attribute.");
    }
    m_id = idAttrib->value();

    xml_attribute<char>* netAttrib = vmNode->first_attribute("net", 0UL, false);
    if(netAttrib == nullptr){
        m_netId = nullptr;
        m_net = nullptr;
    }
    else{
        m_netId = netAttrib->value();
    }
    
    xml_node<char>* imageNode = vmNode->first_node("image", 0UL, false);
    if(imageNode == nullptr){
        throw VirtualMachineException("Virtual machine \"" + m_id + "\" does not specify image.");
    }
    m_image = imageNode->value();

    xml_node<char>* initNode = vmNode->first_node("init", 0UL, false);
    if(initNode != nullptr){
        xml_node<char>* hostnameNode = initNode->first_node("hostname", 0UL, false);
        if(hostnameNode != nullptr)
            m_hostname = hostnameNode->value();

        xml_node<char>* motdNode = initNode->first_node("motd", 0UL, false);
        if(motdNode != nullptr)
            m_motd = motdNode->value();

        xml_node<char>* installFileNode = initNode->first_node("install-file", 0UL, false);
        while(installFileNode != nullptr){
            InstallFile iF(installFileNode, m_presentation);
            if(iF.vmPath.isEmpty()){
                qWarning() << "<install-file> node must have valid <path> subnode. Ignoring objective.";
            }
            else{
                m_installFiles.append(iF);
            }
            installFileNode = installFileNode->next_sibling("install-file", 0UL, false);
        }

    }

    xml_node<char>* objectivesNode = vmNode->first_node("objectives", 0UL, false);
    if(objectivesNode != nullptr){
        xml_node<char>* objectiveNode = objectivesNode->first_node(nullptr, 0UL, false);
        while(objectiveNode){
            if(QString(objectiveNode->name()).toLower() == "file"){
                FileObjective fO(objectiveNode);
                if(fO.vmPath.isEmpty()){
                    qWarning() << "<file> node must have valid <path> subnode. Ignoring objective.";
                }
                else if(fO.regexMatch.isNull() == false && QRegularExpression(fO.regexMatch).isValid() == false){
                    qWarning() << "<file> node contains <regex-march> subnode with invalid regex. Ignoring objective.";
                }
                else{
                    m_fileObjectives.append(fO);
                }
            }
            else if(QString(objectiveNode->name()).toLower() == "run-command"){
                CommandObjective cO(objectiveNode);
                xml_node<char>* commandNode = objectiveNode->first_node("command", 0UL, false);
                if(cO.command.isEmpty()){
                    qWarning() << "<run-command> node must have valid <command> subnode. Ignoring objective.";
                }
                else{
                    m_commandObjectives.append(cO);
                }
            }
            else{
                qWarning() << "Unknown objective node <" + QString(objectiveNode->name()) + ">. Ignoring.";
            }

            objectiveNode = objectiveNode->next_sibling(nullptr, 0UL, false);
        }
    }

    if(m_hostname.isEmpty())
        m_hostname = m_id;
    
    m_cid = cidCounter++;
    createImageFile();
}

VirtualMachine::VirtualMachine(QString id, Network* net, bool hasSlirpNetDev, QString image, Presentation* pres) : m_presentation(pres) {
    m_cid = cidCounter++;
    m_id = id;
    m_net = net;
    m_netId = net->id();
    m_hasSlirpNetDev = hasSlirpNetDev;
    m_image = image;
    m_macAddress = m_net->generateNewMacAddress();
    m_hostname = m_id;
    
    createImageFile();
}

void VirtualMachine::createImageFile(){
    m_diskImage = Config::getDiskImage(m_image);
    if(m_diskImage == nullptr){
        throw VirtualMachineException("Could not create vm \"" + m_id + "\": image \"" + m_image + "\" does not exist");
    }

    if(m_imageFile.open() == false){
        throw VirtualMachineException("Could not create temporary file: " + m_imageFile.errorString());
    }
    QFile orginalDiskFile(m_diskImage->path);
    if(orginalDiskFile.open(QIODevice::ReadOnly) == false){
        QString exceptionStr = "Could not open disk image \"" + m_diskImage->path + "\": ";
        exceptionStr += orginalDiskFile.errorString();
        throw VirtualMachineException(exceptionStr);
    }
    m_imageFile.write(orginalDiskFile.readAll());
    orginalDiskFile.close();
    m_imageFile.close();
}

QStringList VirtualMachine::getArgs(){
    QStringList ret;
    ret << "-machine" << "microvm,acpi=off";
    if(Config::getKvmEnabled()){
        ret << "-enable-kvm" << "-cpu" << "host";
    }
    ret << "-smp" << QString::number(Config::getGuestProcCount())
        << "-m" << QString::number(Config::getGuestMemSize()) + "M"
        << "-mem-prealloc" << "-no-reboot"
        << "-kernel" << Config::getGuestKernelPath()
        << "-append" << KERNEL_DEFAULT_CMD + m_diskImage->initSysPath
        << "-nodefaults" << "-no-user-config" << "-nographic"
        << "-device" << "virtio-serial-device"
        << "-chardev" << "socket,id=char0,path=" + m_consoleServer->fullServerName()
        << "-serial" << "chardev:char0"
        << "-drive" << "id=root,file=" + m_imageFile.fileName() + ",format=qcow2,if=none"
        << "-device" << "virtio-blk-device,drive=root"
        << "-device" << "vhost-vsock-device,guest-cid=" + QString::number(m_cid);
    if(m_net && !m_macAddress.isEmpty()){
        ret << "-netdev" << "socket,id=eth0,localaddr=127.0.0.1,mcast=" VNET_MCAST_ADDR ":" + QString::number(m_net->mcastPort())
            << "-device" << "virtio-net-device,netdev=eth0,mac=" + m_macAddress;
    }
    if(m_hasSlirpNetDev){
        ret << "-netdev" << "user,id=eth1,net=100.127.254.0/24,dhcpstart=100.127.254.8"
            << "-device" << "virtio-net-device,netdev=eth1,mac=00:00:00:00:00:01";
    }
    
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
    QLocalSocket* conn = m_consoleServer->nextPendingConnection();
    if(m_consoleSocket){
        m_terminalSockets.append(conn);
        connect(conn, &QLocalSocket::readyRead, this, [this, conn]{ handleClientConsoleSockReadReady(conn); });
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

    m_consoleServer = new QLocalServer();
    m_serverName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_consoleServer->listen(m_serverName);
    
    m_guestBridge = new GuestBridge(this);

    if(m_consoleServer->isListening() == false) {
        QMessageBox(QMessageBox::Critical,
            "Virtual Slides: VM error",
            "Failed to start VM: " + m_consoleServer->errorString(),
            QMessageBox::Ok,
            nullptr
        ).exec();
        return;
    }

    connect(m_consoleServer, SIGNAL(newConnection()),
        this, SLOT(handleNewConsoleSocketConnection(void)));

    m_vmProcess = new QProcess();

    m_vmProcess->setProgram("qemu-system-x86_64");
    m_vmProcess->setArguments(getArgs());
    m_vmProcess->start();

    connect(m_vmProcess, &QProcess::started, m_guestBridge, &GuestBridge::start);
    connect(m_vmProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(handleVmProcessFinished(int)));

    m_isRunning = true;
}

void VirtualMachine::handleVmProcessFinished(int exitCode) {
    if(m_guestBridge){
        m_guestBridge->stop();
        m_guestBridge->deleteLater();
    }
    m_guestBridge = nullptr;

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

    if(exitCode != 0){
        QMessageBox(QMessageBox::Critical,
            "Virtual Slides: VM error",
            "QEMU failed: " + m_vmProcess->readAllStandardError(),
            QMessageBox::Ok,
            nullptr
        ).exec();
    }

    m_isRunning = false;

    m_vmProcess->deleteLater();
    m_vmProcess = nullptr;

    emit vmStopped();
    if(m_shouldRestart) {
        m_shouldRestart = false;
        start();
    }
}

void VirtualMachine::stop() {
    if(!m_isRunning || !m_vmProcess)
        return;

    if(m_guestBridge){
        m_guestBridge->stop();
        m_guestBridge->deleteLater();
    }
    m_guestBridge = nullptr;

    if(m_vmProcess->state() == QProcess::Running){
        m_vmProcess->terminate();
    }
}

void VirtualMachine::restart() {
    m_shouldRestart = true;
    stop();
}

void VirtualMachine::handleClientConsoleSockReadReady(QLocalSocket* sock) {
    m_consoleSocket->write(sock->readAll());
}

void VirtualMachine::handleConsoleSockReadReady() {
    QByteArray data = m_consoleSocket->readAll();

    for (auto term : m_terminalSockets) {
        term->write(data);
        term->flush();
    }
}

VirtualMachine::~VirtualMachine() {
    stop();
    if(m_guestBridge){
        m_guestBridge->stop();
        m_guestBridge->deleteLater();
    }
    m_guestBridge = nullptr;
}