#include "Network.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>

using namespace rapidxml;

/* 
 * Regex expression for checking validity of subnet string or an ip address
 * ^((25[0-5]|(2[0-4]|1\d|[1-9]|)\d)\.?\b){4}(\/((\d)|([1-2]\d)|30))?$
*/
#define SUBNET_REGEX "^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}(\\/((\\d)|([1-2]\\d)|30))?$"

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

    xml_node<char>* subnetNode = networkNode->first_node("subnet", 0UL, false);
    if(subnetNode != nullptr){
        QRegularExpression subnetRegex(SUBNET_REGEX);
        assert(subnetRegex.isValid());
        if(subnetRegex.match(subnetNode->value()).hasMatch()){
            m_subnet = subnetNode->value();
        }
        else{
            qWarning() << "Subnet specified in <subnet> node is not valid." 
                << "Subnet should be in format x.y.z.w/CIDR."
                << "Defaulting to 10.0.64.0/24.";
        }
    }

    xml_node<char>* vmIdNode = networkNode->first_node("vm-id", 0UL, false);
    if(vmIdNode != nullptr){
        m_vmId = vmIdNode->value();
    }

    m_dhcpServerEnabled = getXmlBoolValue(networkNode, "dhcp-server", true);
    m_Wan = getXmlBoolValue(networkNode, "WAN", true);
}
