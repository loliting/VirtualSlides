#include "VSockServer.hpp"

#include <QtCore/QDebug>
#include <QtCore/QQueue>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QVector>
#include <QtCore/qsystemdetection.h>

#include <sys/socket.h>
#include <unistd.h>

#ifdef Q_OS_LINUX
#include <linux/vm_sockets.h>
#elif defined(Q_OS_MACOS)
#include <sys/vsock.h>
#endif

VSockServer::VSockServer(QObject *parent) : QObject(parent) { }

VSockServer::~VSockServer() {
    close();
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


    if(!VSock::setBlocking(m_sockfd, false)){
        m_err = errno;
        m_errStr = strerror(m_err);
        emit errorOccurred(m_err);
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
        delete addr;
        addr = nullptr;
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }

    if(::listen(m_sockfd, 0) == -1){
        delete addr;
        addr = nullptr;
        m_err = errno;
        m_errStr = strerror(m_err);
        return false;
    }


    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, [=]{ waitForNewConnection(0); });

    m_listening = true;
    return true;
}

VSock* VSockServer::nextPendingConnection() {
    if(m_pendingConnection.isEmpty())
        return nullptr;
    
    VSock* sock = m_pendingConnection.dequeue();

    return sock;
}

void VSockServer::close() {
    if(!m_listening)
        return;

    for(auto client : m_clients) {
        client->disconnect(this, nullptr);
        client->deleteLater();
    }

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

bool VSockServer::waitForNewConnection(int msec, bool *timedOut) {
    auto cleanUp = [=](bool didTimeOut) {
        if(timedOut)
            *timedOut = didTimeOut;
        if(msec != 0){
            m_readNotifier->setEnabled(true);
            VSock::setBlocking(m_sockfd, false);
        }
    };

    if(!m_listening)
        return false;
        
    if(msec != 0){
        VSock::setBlocking(m_sockfd, true);
        m_readNotifier->setEnabled(false);
    }

    int fd = ::accept(m_sockfd, NULL, NULL);
    
    if(fd == -1){
        cleanUp(errno == EAGAIN);
        if(errno != EAGAIN){
            m_err = errno;
            m_errStr = strerror(m_err);
            ::close(fd);
            emit errorOccurred(m_err);
        }
        return false;
    }
    
    VSock *sock = new VSock(this);
    if(!sock->setFd(fd)){
        m_err = errno;
        m_errStr = strerror(m_err);
        ::close(fd);
        emit errorOccurred(m_err);
        sock->deleteLater();
        cleanUp(false);
        return false;
    }

    connect(sock, &VSock::destroyed, this, [=] {
        sock->disconnect(this, nullptr);
        m_clients.remove(sock);
        m_pendingConnection.removeAll(sock);
    });

    m_clients.insert(sock);
    m_pendingConnection.enqueue(sock);

    emit newConnection();
    
    cleanUp(false);
    return true;
}
