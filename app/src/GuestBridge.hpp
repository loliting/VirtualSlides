#ifndef GUESTBRIDGE_HPP
#define GUESTBRIDGE_HPP

#include <QtCore/QObject>

class GuestBridge;

#include "VirtualMachine.hpp"

class GuestBridge : public QObject {
    Q_OBJECT
public:
    GuestBridge(VirtualMachine* vm);
    
private slots:
    void handleVmSockReadReady();
private:
    VirtualMachine* m_vm;
};

#endif // GUESTBRIDGE_HPP