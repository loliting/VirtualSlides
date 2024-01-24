#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

class NetworkManager;

#include "Network.hpp"
#include "VirtualMachineManager.hpp"

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    NetworkManager(QString netsXmlPath);
    Network* getNetwork(QString id) const { return m_networks.value(id, nullptr); }
    void setVirtualMachineManager(VirtualMachineManager* vmManager);
private:
    QMap<QString, Network*> m_networks;
    VirtualMachineManager* m_vmManager = nullptr;
};

#endif // NETWORKMANAGER_HPP