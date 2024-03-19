#ifndef GUESTBRIDGE_HPP
#define GUESTBRIDGE_HPP

#include <QtCore/QObject>

class GuestBridge;

#include "VirtualMachine.hpp"

#include "third-party/nlohmann/json.hpp"

class GuestBridge : public QObject {
    Q_OBJECT
public:
    GuestBridge(VirtualMachine* vm);
    
private:
    enum ResponseStatus {
        Ok,
        Err
    };
    void parseRequest(QString request);
    nlohmann::json statusResponse(ResponseStatus status, std::string errStr = "");
private slots:
    void handleVmSockReadReady();
private:
    VirtualMachine* m_vm;
    QString requestStr;
};

#endif // GUESTBRIDGE_HPP