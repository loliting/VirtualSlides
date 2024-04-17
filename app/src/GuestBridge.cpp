#include "GuestBridge.hpp"

#include <QtWidgets/QMessageBox>

using namespace nlohmann;


GuestBridge::GuestBridge(VirtualMachine* vm) : QObject(vm) {
    m_vm = vm;

    m_server = new VSockServer(this);
    if(!m_server->listen(VSockServer::Host, m_vm->m_cid)){
        qWarning() << "VSockServer failed:" << m_server->errorString();
        return;
    }

    connect(m_server, &VSockServer::newConnection, this, [=] {
        VSock* sock = m_server->nextPendingConnection();
        connect(sock, &VSock::readyRead, [=] {
            handleVmSockReadReady(sock);
        });
        connect(sock, &VSock::errorOccurred, [=] {
            qWarning() << "An error occurred on vsock:" << sock->errorString();
            sock->close();
            sock->deleteLater();
        });
    });

}

GuestBridge::~GuestBridge() {
    if(m_server){
        m_server->deleteLater();
    }
}

void GuestBridge::parseRequest(VSock* sock, QString request) {
    std::string requestType;
    json response;
    try{
        json jsonRequest = json::parse(request.toStdString());
        requestType = jsonRequest["type"];
    }
    catch(std::exception &e){
        response.update(statusResponse(ResponseStatus::Err, e.what()));
        QMessageBox msgBox(QMessageBox::Critical,
            "Virtual Slides",
            "Communication with virtual machine failed:\n" + QString(e.what()),
            QMessageBox::Ok);
        msgBox.exec();
    }

    if (requestType.empty()){ }
    else if(requestType == "reboot"){
        m_vm->m_shouldRestart = true;
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if(requestType == "download-test") {
        response["download-test"] = std::string(1024 * 1024, 'a');
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if(requestType == "hostname") {
        response["hostname"] = m_vm->m_hostname.toStdString();
        response.update(statusResponse(ResponseStatus::Ok));
    } 
    else {
        std::string err = "Unknown request type: \"" + requestType + "\"";
        response.update(statusResponse(ResponseStatus::Err, err));
    }

    QByteArray jsonResponseStr = QByteArray::fromStdString(response.dump()) + "\x1e";
    sock->write(jsonResponseStr);
    connect(sock, &VSock::bytesWritten, this, [=] {
        if(sock->bytesToWrite() <= 0){
            sock->close();
            sock->disconnect();
            sock->deleteLater();
        }
    });
}

void GuestBridge::handleVmSockReadReady(VSock* sock) {
    requestStr += sock->readAll();

    while(requestStr.contains("\x1e") != false){
        QString request = requestStr;
        request.truncate(requestStr.indexOf("\x1e"));
        parseRequest(sock, request);
        requestStr.remove(0, request.length() + 1);
    }
}

json GuestBridge::statusResponse(ResponseStatus status, std::string errStr) {
    json response;
    switch(status) {
    case ResponseStatus::Ok:
        response["status"] = "ok";
        break;
    case ResponseStatus::Err:
        response["status"] = "err";
        response["error"] = errStr;
        break;
    }
    return response;
}