#ifndef GUESTBRIDGE_HPP
#define GUESTBRIDGE_HPP

#include <QtCore/QObject>

class GuestBridge;

#include "VirtualMachine.hpp"
#include "VSockServer.hpp"

#include "third-party/nlohmann/json.hpp"

class GuestBridge : public QObject {
    Q_OBJECT
public:
    GuestBridge(VirtualMachine* vm);
    ~GuestBridge();
    
private:
    enum ResponseStatus {
        Ok,
        Err
    };
    void parseRequest(VSock* sock, QString request);
    nlohmann::json statusResponse(ResponseStatus status, std::string errStr = "");
private slots:
    void handleVmSockReadReady(VSock* sock);
private:
    VirtualMachine* m_vm = nullptr;
    
    VSockServer* m_server = nullptr;

    QString requestStr;
};

#endif // GUESTBRIDGE_HPP