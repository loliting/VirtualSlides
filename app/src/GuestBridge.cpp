#include "GuestBridge.hpp"

#include <QtWidgets/QMessageBox>

#include "third-party/nlohmann/json.hpp"

using namespace nlohmann;

GuestBridge::GuestBridge(VirtualMachine* vm) {
    m_vm = vm;

    connect(m_vm->m_vmReadSocket, SIGNAL(readyRead()),
        this, SLOT(handleVmSockReadReady(void)));
}

void GuestBridge::parseRequest(QString request) {
    std::string requestType;
    try{
        json jsonRequest = json::parse(request.toStdString());
        requestType = jsonRequest["type"];
    }
    catch(std::exception &e){
        QMessageBox msgBox(QMessageBox::Critical,
            "Virtual Slides",
            "Communication with virtual machine failed:\n" + QString(e.what()),
            QMessageBox::Ok);
        msgBox.exec();
    }

    if(requestType == "reboot"){
        m_vm->m_shouldRestart = true;
        sendStatusResponse(ResponseStatus::Ok);
    }
}

void GuestBridge::handleVmSockReadReady() {
    requestStr += m_vm->m_vmReadSocket->readAll();
    while(requestStr.contains("\n") != false){
        QString request = requestStr;
        request.truncate(requestStr.indexOf('\n') + 1);
        qDebug() << request;
        parseRequest(request);
        requestStr.remove(0, request.length());
    }
}

void GuestBridge::sendStatusResponse(ResponseStatus status, QString errStr) {
    json jsonResponse;
    switch(status) {
    case ResponseStatus::Ok:
        jsonResponse["status"] = "ok";
        break;
    case ResponseStatus::Err:
        jsonResponse["status"] = "err";
        jsonResponse["error"] = errStr.toStdString();
        break;
    }
    std::string jsonResponseStr = jsonResponse.dump() + "\n";
    m_vm->m_vmWriteSocket->write(jsonResponseStr.c_str(), jsonResponseStr.length());
    m_vm->m_vmWriteSocket->flush();
}