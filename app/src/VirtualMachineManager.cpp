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
        xmlDoc.parse<parse_trim_whitespace | parse_normalize_whitespace>(ba.data());
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
            qWarning() << "vms.xml:" << e.cause();
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