#include "NetworkManager.hpp"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QUuid>

#include "third-party/RapidXml/rapidxml.hpp"

#include "Presentation.hpp"

using namespace rapidxml;

NetworkManager::NetworkManager(QString netsXmlPath){

    QFile netsXml(netsXmlPath);

    if(netsXml.open(QIODevice::ReadOnly) == false){
        qWarning() << "File nets.xml does not exists inside the archive";
        return;
    }
    
    QByteArray ba = netsXml.readAll();
    
    xml_document<char> xmlDoc;
    try{
        xmlDoc.parse<parse_trim_whitespace | parse_normalize_whitespace>(ba.data());
    }
    catch(parse_error& e){
        qWarning() << "Failed to parse nets.xml: " + QString(e.what());
        return;
    }

    xml_node<char>* rootNode = xmlDoc.first_node("Networks", 0UL, false);
    if(rootNode == nullptr){
        qWarning() << "Failed to parse nets.xml: root <Networks> node does not exist.";
        return;
    }

    xml_node<char>* netNode = rootNode->first_node(nullptr, 0UL, false);

    while(netNode){
        if(QString(netNode->name()).toLower() != "net"){
            qWarning() << "Unknown node: <" + QString(netNode->name()) + ">. Ignoring.";
            netNode = netNode->next_sibling(nullptr, 0UL, false);
            continue;
        }
        Network* net = nullptr;
        try{
            net = new Network(netNode);
            if(net->m_id.isEmpty()){
                throw NetworkException("Specified network id is not valid.");
            }
            if(m_networks.contains(net->m_id)){
                throw NetworkException("Network with ID: " + net->m_id + " already exist.");
            }
            m_networks[net->m_id] = net;
        }
        catch(NetworkException &e){
            if(net){
                delete net;
            }
            qWarning() << "nets.xml:" << e.cause() << "Ignoring network.";
        }

        netNode = netNode->next_sibling(nullptr, 0UL, false);
    }

    netsXml.close();
}

void NetworkManager::setVirtualMachineManager(VirtualMachineManager* vmManager){
    assert(m_vmManager == nullptr);
    assert(vmManager);

    for(auto net : m_networks){
        if(net->m_vmId != nullptr){
            net->m_vm = vmManager->getVirtualMachine(net->m_vmId);
            if(net->m_vm == nullptr){
                QString exceptionStr = "nets.xml: net \"" + net->m_id + "\": ";
                exceptionStr += "Virtual machine \"" + net->m_vmId + "\" does not exist.";
                throw PresentationException(exceptionStr);
            }
        }
        else{
            net->m_vmId = QUuid().toString();
            net->m_vm = new VirtualMachine(net, net->m_vmId, "Router", true);
        }
    }
}
