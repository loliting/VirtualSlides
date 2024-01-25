#include "VirtualMachine.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>

#include "Application.hpp"
#include "DiskImageManager.hpp"

using namespace rapidxml;

#define KERNEL_DEFAULT_CMD "acpi=off reboot=t panic=-1 console=ttyS0 root=/dev/vda rw init=/sbin/vs_init selinux=0"
#define KERNEL_QUIET_CMD KERNEL_DEFAULT_CMD " quiet"
#define KERNEL_EARLYPRINTK_CMD KERNEL_DEFAULT_CMD " earlyprintk=ttyS0"

static bool getXmlBoolAttrib(xml_node<char>* node, const char* name, bool defaultValue){
    if(node == nullptr){
        return defaultValue;
    }

    xml_attribute<char>* attrib = node->first_attribute(name, 0UL, false);
    if(attrib == nullptr){
        return defaultValue;
    }

    if(QString(attrib->value()).toLower() == "true"){
        return true;
    }
    else if(QString(attrib->value()).toLower() == "false"){
        return false;
    }
    else{
        return defaultValue;
    }
}

static bool getXmlBoolValue(xml_node<char>* parentNode, const char* name, bool defaultValue){
    if(parentNode == nullptr){
        return defaultValue;
    }

    xml_node<char>* node = parentNode->first_node(name, 0UL, false);
    if(node == nullptr){
        return defaultValue;
    }

    if(QString(node->value()).toLower() == "true"){
        return true;
    }
    else if(QString(node->value()).toLower() == "false"){
        return false;
    }
    else{
        return defaultValue;
    }
}

InstallFile::InstallFile(xml_node<char>* installFileNode){
    xml_node<char>* pathNode = installFileNode->first_node("path", 0UL, false);
    vmPath = pathNode->value();
    xml_node<char>* contentNode = installFileNode->first_node("content", 0UL, false);
    xml_attribute<char>* contentPathNode = nullptr;
    if(contentNode != nullptr){
        content = contentNode->value();
        contentPathNode = contentNode->first_attribute("path", 0UL, false);
    }
    if(contentPathNode != nullptr){
        hostPath = contentPathNode->value();
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

VirtualMachine::VirtualMachine(xml_node<char>* vmNode){
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
        if(hostnameNode != nullptr){
            m_hostname = hostnameNode->value();
        }

        m_dhcpClient = getXmlBoolValue(initNode, "dhcp-client", true);

        xml_node<char>* installFileNode = initNode->first_node("install-file", 0UL, false);
        while(installFileNode != nullptr){
            InstallFile iF(installFileNode);
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

    createImageFile();
    createWidget();
}

VirtualMachine::VirtualMachine(QString id, Network* net, bool hasSlirpNetDev, bool dhcpServer, QString image){
    m_id = id;
    m_net = net;
    m_netId = net->id();
    m_hasSlirpNetDev = hasSlirpNetDev;
    m_dhcpServer = dhcpServer;
    m_image = image;

    createImageFile();
    createWidget();
}

void VirtualMachine::createImageFile(){
    DiskImage* diskImage = DiskImageManager::getDiskImage(m_image);
    if(diskImage == nullptr){
        throw VirtualMachineException("Could not create vm \"" + m_id + "\": image \"" + m_image + "\" does not exist");
    }

    if(m_imageFile.open() == false){
        throw VirtualMachineException("Could not create temporary file: " + m_imageFile.errorString());
    }
    QFile orginalDiskFile(diskImage->path);
    if(orginalDiskFile.open(QIODevice::ReadOnly) == false){
        QString exceptionStr = "Could not open disk image \"" + diskImage->path + "\": ";
        exceptionStr += orginalDiskFile.errorString();
        throw VirtualMachineException(exceptionStr);
    }
    m_imageFile.write(orginalDiskFile.readAll());
    orginalDiskFile.close();
}

void VirtualMachine::createWidget(){
    assert(m_imageFile.fileName() != nullptr);

    m_widget = new QTermWidget(0, nullptr);
    m_widget->setArgs(getArgs());
    m_widget->setShellProgram("qemu-system-x86_64");
    m_widget->setAutoClose(false);
}

QStringList VirtualMachine::getArgs(){
    QStringList ret;
    ret << "-M" << "microvm"
        // FIXME: add -enable-kvm flag on kvm enabled hosts, change cpu type to host
        << "-cpu" << "max"
        << "-m" << "128m"
        << "-no-acpi" << "-no-reboot"
        /* 
         * FIXME: Implement some sort of kernel manager,
         * so we can track where the kernel actually is, instead of hardcoding
         * path, track kernel versions and store multiple releases
         */
        << "-kernel" << Application::applicationDirPath() + "/bzImage"
        << "-append" << KERNEL_DEFAULT_CMD
        << "-nodefaults" << "-no-user-config" << "-nographic"
        << "-serial" << "mon:stdio"
        << "-drive" << "id=root,file=" + m_imageFile.fileName() + ",format=qcow2,if=none"
        << "-device" << "virtio-blk-device,drive=root";
        if(m_hasSlirpNetDev){
            ret << "-netdev" << "user,id=net0,net=100.127.254.0/24,dhcpstart=100.127.254.8"
                << "-device" << "virtio-net-device,netdev=net0";
        }
        ret << "-netdev" << "socket,id=net1,localaddr=127.0.0.1,mcast=224.0.0.69:1234"
        << "-device" << "virtio-net-device,netdev=net1,mac=42:69:00:00:00:01";

    return ret;
}
