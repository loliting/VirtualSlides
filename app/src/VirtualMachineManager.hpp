#ifndef VIRTUALMACHINEMANAGER_HPP
#define VIRTUALMACHINEMANAGER_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

#include "VirtualMachine.hpp"

class VirtualMachineManager : QObject
{
    Q_OBJECT
public:
    VirtualMachineManager(QString vmsXmlPath);
    ~VirtualMachineManager();
private:
    QMap<QString, VirtualMachine*> m_virtualMachines;
};

#endif // VIRTUALMACHINEMANAGER_HPP