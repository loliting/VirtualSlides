#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <QtCore/QObject>
#include <QtCore/QString>

#include <exception>

#include "third-party/RapidXml/rapidxml.hpp"

class Network;

#include "VirtualMachine.hpp"

class NetworkException : std::exception
{
public:
    NetworkException(QString cause) { m_what = cause.toStdString(); }
    QString cause() const throw() { return QString::fromStdString(m_what); }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};

class Network : QObject
{
    Q_OBJECT
public:
    Network(rapidxml::xml_node<char>* networkNode);

    QString generateNewMacAddress();

    QString id() const { return m_id; }
    VirtualMachine* vm() const { return m_vm; }
    bool hasWan() const { return m_Wan; }
    bool hasDhcpServerEnabled() const { return m_dhcpServerEnabled; }
    QString subnet() const { return m_subnet; }
private:
    QString m_id;
    QString m_subnet = "10.0.64.0/24";
    QString m_vmId;
    VirtualMachine* m_vm = nullptr;
    bool m_dhcpServerEnabled = true;
    bool m_Wan = true;

    quint32 macAddressesCount = 1;

    friend class NetworkManager;
};

#endif // NETWORK_HPP