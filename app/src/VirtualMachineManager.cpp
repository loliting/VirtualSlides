#include "VirtualMachineManager.hpp"

#include <QtCore/QFile>
#include <QtCore/QDebug>

#include "third-party/RapidXml/rapidxml.hpp"

#include "Presentation.hpp"

using namespace rapidxml;

VirtualMachineManager::VirtualMachineManager(QString vmsXmlPath) {
    QFile vmsXml(vmsXmlPath);

    if(vmsXml.open(QIODevice::ReadOnly) == false){
        qWarning() << "File vms.xml does not exists inside the archive";
        return;
    }
    
    QByteArray ba = vmsXml.readAll();
    
    xml_document<char> xmlDoc;
    try{
        xmlDoc.parse<parse_normalize_whitespace>(ba.data());
    }
    catch(parse_error& e){
        qWarning() << "Failed to parse vms.xml: " + QString(e.what());
        return;
    }

    xml_node<char>* rootNode = xmlDoc.first_node("VirtualMachines", 0UL, false);
    if(rootNode == nullptr){
        qWarning() << "Failed to parse vms.xml: root <VirtualMachines> node does not exist.";
        return;
    }

    xml_node<char>* vmNode = rootNode->first_node("vm", 0UL, false);

    while(vmNode){
        if(QString(vmNode->name()).toLower() != "vm"){
            qWarning() << "Unknown node: <" + QString(vmNode->name()) + ">. Ignoring.";
            vmNode = vmNode->next_sibling(nullptr, 0UL, false);
            continue;
        }
        VirtualMachine* vm = nullptr;
        try{
            vm = new VirtualMachine(vmNode);
            if(vm->m_id.isEmpty()){
                throw VirtualMachineException("Specified vm id is not valid.");
            }
            if(m_virtualMachines.contains(vm->m_id)){
                throw VirtualMachineException("Virtual machine with ID: " + vm->m_id + " already exist.");
            }
            m_virtualMachines[vm->m_id] = vm;
        }
        catch(VirtualMachineException &e){
            if(vm){
                delete vm;
            }
            qWarning() << "vms.xml:" << e.cause() << "Ignoring vm.";
        }

        vmNode = vmNode->next_sibling(nullptr, 0UL, false);
    }

    vmsXml.close();
}

VirtualMachineManager::~VirtualMachineManager(){
    for(auto vm : m_virtualMachines){
        delete vm;
    }
}

void VirtualMachineManager::setNetworkManager(NetworkManager* netManager){
    assert(m_netManager == nullptr);
    assert(netManager);

    m_netManager = netManager;
    
    for(auto vm : m_virtualMachines){
        Network* net = netManager->getNetwork(vm->m_netId);
        if(net){
            if(net->vm() == vm){
                vm->m_hasSlirpNetDev = net->hasWan();
                vm->m_dhcpServer = net->hasDhcpServerEnabled();
            }
            vm->setNet(net);
            if(vm->m_hasSlirpNetDev)
                vm->start();
        }
        else if(vm->m_netId != nullptr){
            QString exceptionStr = "vms.xml: vm \"" + vm->m_id + "\": ";
            exceptionStr += "Network \"" + vm->m_netId + "\" does not exist.";
            throw PresentationException(exceptionStr);
        }
    }
}

VirtualMachine* VirtualMachineManager::addVm(QString id, Network* net, bool hasSlirpNetDev, bool dhcpServer, QString image){
    assert(m_virtualMachines.contains(id) == false);

    VirtualMachine* vm = new VirtualMachine(id, net, hasSlirpNetDev, dhcpServer, image);

    m_virtualMachines[vm->m_id] = vm;
    
    return vm;
}