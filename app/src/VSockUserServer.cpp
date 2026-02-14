#include "VSockUserServer.hpp"

#include "UnixSocketServer.hpp"
#include "UnixSocket.hpp"
#include "VSockUser.hpp"

bool VSockUserServer::listen(const QString &path, uint32_t cid, uint32_t port) {
    m_cid = cid;
    m_port = port;

    m_server = new UnixSocketServer(this);
    if(!m_server->listen(path)) {
        close();
        return false;
    }
    setProperty("_unixSocketServer", QVariant::fromValue<UnixSocketServer*>(m_server));

    connect(m_server, &UnixSocketServer::errorOccurred, this, [this]() {
        if(!m_server->isListening()) {
            close();
        }
        m_errStr = m_server->errorString();
        emit errorOccurred(m_err = m_server->error());
    });
    connect(m_server, &UnixSocketServer::newConnection, this, [this]() {
        UnixSocket* sock = m_server->nextPendingConnection();
        if(!sock) return;
        connect(sock, &UnixSocket::disconnected, this, [this, sock]() {
            sock->deleteLater();
        });
        connect(sock, &UnixSocket::readyRead, this, [this, sock]() {
            const size_t connectionDataSize = sizeof(VSockUser::VSockUserConnectionKey);
            if(sock->bytesAvailable() < connectionDataSize) {
                return;
            }
            VSockUser* vsock = new VSockUser(this);
            sock->readData((char*)&vsock->connectionData, connectionDataSize);
            vsock->open(QIODevice::ReadWrite);
            
            VSockUser::ConnectionReply reply;
            if(!(m_cid == SpecialCIDs::Any || m_cid == vsock->connectionData.host_cid) || m_port != vsock->connectionData.host_port) {
                reply = VSockUser::ConnectionReply::Reject;
                sock->writeData((const char*)&reply, sizeof(reply));
                sock->waitForBytesWritten(-1);
                vsock->deleteLater();
                sock->deleteLater();
                return;
            }
            reply = VSockUser::ConnectionReply::Accept;
            sock->writeData((const char*)&reply, sizeof(reply));
            vsock->setSock(sock);
            m_pendingConnection.enqueue(vsock);
            disconnect(sock, nullptr, this, nullptr);
            emit newConnection();
        });
    });

    m_listening = true;
    return true;
}

void VSockUserServer::close() {
    // unix server will close any remaining connections
    m_pendingConnection.clear();

    if(m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
        setProperty("_unixSocketServer", QVariant::fromValue<UnixSocketServer*>(nullptr));
    }
    m_cid = 0;
    m_port = 0;
}

QString VSockUserServer::serverName() const {
    if(m_server)
        return m_server->serverName();
    return nullptr;
}

QString VSockUserServer::fullServerName() const {
    if(m_server)
        return m_server->fullServerName();
    return nullptr;
}

bool VSockUserServer::isListening() const {
    return m_listening && m_server->isListening();
}
