#include "GuestBridge.hpp"

#include <QtWidgets/QMessageBox>

using namespace nlohmann;


GuestBridge::GuestBridge(VirtualMachine* vm) {
    m_vm = vm;

    connect(m_vm->m_vmReadSocket, SIGNAL(readyRead()),
        this, SLOT(handleVmSockReadReady(void)));
}

void GuestBridge::parseRequest(QString request) {
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
        response["download-test"] = std::string(1000 * 1000, 'a');
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

    std::string jsonResponseStr = response.dump();
    m_vm->m_vmWriteSocket->write(QByteArray::fromStdString(jsonResponseStr) + "\x1e\n");
    m_vm->m_vmWriteSocket->flush();
}

void GuestBridge::handleVmSockReadReady() {
    requestStr += m_vm->m_vmReadSocket->readAll();
    while(requestStr.contains("\x1e") != false){
        QString request = requestStr;
        request.truncate(requestStr.indexOf("\x1e"));
        parseRequest(request);
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