#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <QtCore/QObject>
#include <QtCore/QString>

#include "third-party/RapidXml/rapidxml.hpp"

#include "VirtualMachine.hpp"

class Network : QObject
{
    Q_OBJECT
public:
    Network(rapidxml::xml_node<char> networkNode) { };
private:
    QString m_id;
    QString m_subnet;
    QString m_vmId;
    VirtualMachine* m_vm;
    bool m_dhcpServer = true;
    bool m_Wan = true;
};

#endif // NETWORK_HPP