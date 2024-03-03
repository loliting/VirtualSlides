#include "GuestBridge.hpp"

#include "third-party/nlohmann/json.hpp"

using namespace nlohmann;

GuestBridge::GuestBridge(VirtualMachine* vm) {
    m_vm = vm;

    connect(m_vm->m_vmReadSocket, SIGNAL(readyRead()),
        this, SLOT(handleVmSockReadReady(void)));
}


void GuestBridge::handleVmSockReadReady() {
    QTextStream(stdout) << m_vm->m_vmReadSocket->readAll();
    QTextStream(stdout).flush();

    json jsonResponse;
    jsonResponse["testField"] = "testValue";
    std::string jsonResponseStr = jsonResponse.dump() + "\n";
    m_vm->m_vmWriteSocket->write(jsonResponseStr.c_str(), jsonResponseStr.length());
    qDebug() << "Wrote" << jsonResponseStr.length() << "bytes";
    m_vm->m_vmWriteSocket->flush();
}