#include "VSockServer.hpp"

#include <QtCore/QDebug>
#include <QtCore/QQueue>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QVector>
#include <QtCore/qsystemdetection.h>

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef Q_OS_LINUX
#include <linux/vm_sockets.h>
#elif defined(Q_OS_MACOS)
#include <sys/vsock.h>
#endif

VSockServer::VSockServer(QObject *parent) : QObject(parent) { }

VSockServer::~VSockServer() {
    for(auto client : m_clients) {
        client->disconnect(this, nullptr);
        client->deleteLater();
    }

    close();
}

void VSockServer::handleClientAvaliable() {
    int fd;

    while((fd = ::accept(m_sockfd, NULL, NULL)) != -1) {
        VSock *sock = new VSock(this);
        if(!sock->setFd(fd)){
            m_err = errno;
            m_errStr = strerror(m_err);
            ::close(fd);
            emit errorOccurred(m_err);
            sock->deleteLater();
            continue;
        }

        m_clients.insert(sock);
        m_pendingConnection.enqueue(sock);

        emit newConnection();
    }
    
    if(fd == -1 && errno != EAGAIN){
        m_err = errno;
        m_errStr = strerror(m_err);
        emit errorOccurred(m_err);
    }
}


bool VSockServer::listen(uint32_t cid, uint32_t port) {
    if(m_listening){
        if(cid == m_cid && m_port == port)
            return true;
        return false;
    }

    if((m_sockfd = socket(AF_VSOCK, SOCK_STREAM, 0)) == -1) {
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }

    if(fcntl(m_sockfd, F_SETFL, O_NONBLOCK) == -1) {
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }
    
    struct sockaddr_vm *addr = new struct sockaddr_vm;
    m_cid = cid;
    addr->svm_cid = cid;
    addr->svm_family = AF_VSOCK;
    m_port = port;
    addr->svm_port = port;
    m_addr = (struct sockaddr*)addr;

    if(::bind(m_sockfd, m_addr, sizeof(struct sockaddr_vm)) == -1) {
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }

    if(::listen(m_sockfd, 0) == -1){
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }


    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, &VSockServer::handleClientAvaliable);

    m_listening = true;
    return true;
}

VSock* VSockServer::nextPendingConnection() {
    if(m_pendingConnection.isEmpty())
        return nullptr;
    
    VSock* sock = m_pendingConnection.dequeue();
    sock->connect(sock, &VSock::disconnected, [=] {
        this->m_clients.remove(sock);
    });

    return sock;
}

void VSockServer::close() {
    if(!m_listening)
        return;
    m_listening = false;
    if(m_readNotifier){
        m_readNotifier->disconnect();
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }

    ::close(m_sockfd);
    delete m_addr;
    m_addr = nullptr;
}