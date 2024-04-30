#include "Network.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QChar>

using namespace rapidxml;

static quint32 portCounter = 3000;
static quint32 macAddressCounter = 0;

const QString Network::macAddressOUI = QStringLiteral("12:34:56");


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
    assert(++macAddressCounter <= 0xFFFFFF);

    return (macAddressOUI + ":%1:%2:%3")
        .arg((macAddressCounter & 0xFF0000) >> 16, 2, 16, (QChar)u'0')
        .arg((macAddressCounter & 0x00FF00) >> 8, 2, 16, (QChar)u'0')
        .arg(macAddressCounter & 0x0000FF, 2, 16, (QChar)u'0');
}