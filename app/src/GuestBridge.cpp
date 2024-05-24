#include "GuestBridge.hpp"

#include <QtWidgets/QMessageBox>

using namespace nlohmann;


GuestBridge::GuestBridge(VirtualMachine* vm) : QObject(vm), m_vm(vm)
{

}

GuestBridge::~GuestBridge() {
    stop();
}

bool GuestBridge::start() {
    if(m_started)
        return true;

    m_server = new VSockServer(this);

    if(!m_server->listen(VSockServer::Host, m_vm->m_cid)) {
        qWarning() << "VSockServer failed:" << m_server->errorString();
        return false;
    }

    connect(m_server, &VSockServer::newConnection, this, [this] {
        VSock* sock = m_server->nextPendingConnection();
        connect(sock, &VSock::readyRead, [sock, this] {
            handleVmSockReadReady(sock);
        });
        connect(sock, &VSock::errorOccurred, [sock] {
            qWarning() << "An error occurred on vsock:" << sock->errorString();
            sock->close();
            sock->deleteLater();
        });
        connect(sock, &VSock::disconnected, [sock] {
            sock->deleteLater();
        });
    });

    m_started = true;
    return true;
}

bool GuestBridge::isListening() {
    return m_started;
}

void GuestBridge::stop() {
    if (!m_started)
        return;
    
    m_server->close();
    if (m_server) {
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_started = false;
}

static json installFileToJson(InstallFile* installFile){
    json json;
    json["content"] = installFile->content;
    json["path"] = installFile->vmPath.toStdString();
    json["uid"] = installFile->owner;
    json["gid"] = installFile->group;
    json["perm"] = installFile->perm;

    return json;
}

void GuestBridge::parseRequest(VSock* sock, QString request) {
    std::string requestType;
    json response, jsonRequest;
    try {
        jsonRequest = json::parse(request.toStdString());
        requestType = jsonRequest["type"];
    }
    catch (std::exception &e) {
        response.update(statusResponse(ResponseStatus::Err, e.what()));
        QMessageBox msgBox(QMessageBox::Critical,
            "Virtual Slides",
            "Communication with virtual machine failed:\n" + QString(e.what()),
            QMessageBox::Ok);
        msgBox.exec();
    }

    if (requestType.empty()) { }
    else if (requestType == "reboot") {
        m_vm->m_shouldRestart = true;
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "downloadTest") {
        response["downloadTest"] = std::string(1024 * 1024 * 32, 'a');
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "getHostname") {
        response["hostname"] = m_vm->m_hostname.toStdString();
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "getInstallFiles") {
        std::vector<json> installFiles;
        for (auto &installFile : m_vm->m_installFiles)
            installFiles.push_back(installFileToJson(&installFile));
        
        response["installFiles"] = installFiles;
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "getInitScripts") {
        std::vector<json> initScripts;
        for (auto &initScript : m_vm->m_initScripts) {
            json json;
            json["content"] = initScript.content;
            initScripts.push_back(json);
        }

        response["initScripts"] = initScripts;
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "getTasks") {
        std::vector<json> tasks;
        for (auto &task : m_vm->m_tasks)
            tasks.push_back(task->toJson());

        response["tasks"] = tasks;
        response.update(statusResponse(ResponseStatus::Ok));
    }
    else if (requestType == "finishSubtask") {
        json taskId = jsonRequest["taskId"];
        json subtaskId = jsonRequest["subtaskId"];
        Task* task = nullptr;
        Subtask* subtask = nullptr;
        if(!taskId.is_string() || !subtaskId.is_string()) {
            response.update(statusResponse(ResponseStatus::Err, 
                "Recived request of type \"finishSubtask\" does not contain " 
                "\"taskId\" and/or \"subtaskId\" field(s)"
            ));
        }
        else {
            task = m_vm->m_tasks[taskId];
            if(task)
                subtask = task->subtasks[subtaskId];
        }

        if(task && subtask){
            subtask->done = true;
            response.update(statusResponse(ResponseStatus::Ok));
        }
        else {
            response.update(statusResponse(ResponseStatus::Err, 
                "Recived request of type \"finishSubtask\" contains " 
                "nonexistant taskId/subtaskId pair"
            ));
        }
    }
    else if (requestType == "getTermSize") {
        response["termHeight"] = m_vm->m_minimumWidgetSize.height();
        response["termWidth"] = m_vm->m_minimumWidgetSize.width();

        response.update(statusResponse(ResponseStatus::Ok));
    }
    else {
        std::string err = "Unknown request type: \"" + requestType + "\"";
        response.update(statusResponse(ResponseStatus::Err, err));
    }

    QByteArray jsonResponseStr = QByteArray::fromStdString(response.dump()) + "\x1e";
    sock->write(jsonResponseStr);
}

void GuestBridge::handleVmSockReadReady(VSock* sock) {
    requestStr += sock->readAll();

    while (requestStr.contains("\x1e") != false) {
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