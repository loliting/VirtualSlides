#ifndef VIRTUALMACHINEMANAGER_HPP
#define VIRTUALMACHINEMANAGER_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

class VirtualMachineManager;

#include "VirtualMachine.hpp"
#include "NetworkManager.hpp"

class VirtualMachineManager : public QObject
{
    Q_OBJECT
public:
    VirtualMachineManager(QString vmsXmlPath, Presentation* pres);
    ~VirtualMachineManager();
    VirtualMachine* getVirtualMachine(QString id) const { return m_virtualMachines.value(id, nullptr); }
    VirtualMachine* addVm(QString id, Network* net, bool hasSlirpNetDev, QString image);

    void setNetworkManager(NetworkManager* netManager);
private:
    QMap<QString, VirtualMachine*> m_virtualMachines;
    NetworkManager* m_netManager = nullptr;

    Presentation* m_presentation;
};

#endif // VIRTUALMACHINEMANAGER_HPP