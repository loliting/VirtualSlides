#ifndef NETWORK_HPP
#define NETWORK_HPP

#define VNET_MCAST_ADDR "224.0.0.69"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringLiteral>

#include <exception>

#include "third-party/nlohmann/json.hpp"

class Network;
class VirtualMachine;

class NetworkException : public std::exception
{
public:
    NetworkException(std::string what) { m_what = what; }
    virtual const char* what() const throw() override { return m_what.c_str(); }
private:
    std::string m_what;
};

class Network : public QObject
{
    Q_OBJECT
public:
    static const QString macAddressOUI;

public:
    Network(nlohmann::json &netObject);

    QString generateNewMacAddress();

    QString id() const { return m_id; }
    uint16_t mcastPort() const { return m_mcastPort; }

    VirtualMachine* vm() const { return m_vm; }
    
    bool hasWan() const { return m_wan; }
private:
    uint16_t m_mcastPort;
    
    QString m_id;
    QString m_vmId;
    VirtualMachine* m_vm = nullptr;
    bool m_wan = true;

    friend class Presentation;
};

#endif // NETWORK_HPP