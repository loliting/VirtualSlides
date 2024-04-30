#include "Network.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>

using namespace rapidxml;

static uint32_t portCounter = 3000;


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

Network::Network(rapidxml::xml_node<char>* networkNode){
    xml_attribute<char>* idAttr = networkNode->first_attribute("id", 0UL, false);

    if(idAttr == nullptr){
        throw NetworkException("<Net> node does not contain id attribute.");
    }
    m_id = idAttr->value();

    xml_node<char>* vmIdNode = networkNode->first_node("vm-id", 0UL, false);
    if(vmIdNode != nullptr){
        m_vmId = vmIdNode->value();
    }

    m_Wan = getXmlBoolValue(networkNode, "WAN", true);
    m_mcastPort = portCounter++;
}

QString Network::generateNewMacAddress() {
    assert(macAddressesCount <= 0xFF);

    QString lastByte = QString::number(macAddressesCount++, 16);
    // normalize last byte to always take 2 characters
    if(lastByte.length() == 1){
        lastByte = "0" + lastByte;
    }

    return QString::number(0x1a, 16) + ":" + QString::number(0x2b, 16) + ":"
         + QString::number(0x3c, 16) + ":" + QString::number(0x4d, 16) + ":"
         + QString::number(0x5e, 16) + ":" + lastByte;
}