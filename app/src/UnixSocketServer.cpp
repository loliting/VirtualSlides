#include "UnixSocketServer.hpp"

#include <QtCore/QDebug>
#include <QtCore/QQueue>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QVector>
#include <QtCore/qsystemdetection.h>

#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

#include "UnixSocket.hpp"

UnixSocketServer::UnixSocketServer(QObject *parent) : QObject(parent) { }

UnixSocketServer::~UnixSocketServer() {
    close();
}

bool UnixSocketServer::listen(QString path) {
    QFileInfo serverPathInfo = QFileInfo(path);
    if(m_listening){
        if(m_serverPath == serverPathInfo.absoluteFilePath() || m_serverName == path)
            return true;

        m_errStr = "Server is already listening on different address";
        emit errorOccurred(m_err = EPERM);
        return false;
    }

    if(!path.contains("/")) {
        path = QDir::tempPath() + "/" + path;
        serverPathInfo = QFileInfo(path);
    }
    m_serverPath = serverPathInfo.absoluteFilePath();
    m_serverName = serverPathInfo.fileName();

    if((m_sockfd = UnixSocket::makeSocket(m_serverPath, &m_addr)) == -1) {
        m_errStr = strerror(m_err = errno);
        emit errorOccurred(m_err);
        return false;
    }

    if (unlink(((struct sockaddr_un*)m_addr)->sun_path) < 0 && errno != ENOENT) {
        close();
        m_errStr = "Failed to unlink socket: ";
        m_errStr += strerror(m_err = errno);
        emit errorOccurred(m_err);
        return false;
    }

    if(::bind(m_sockfd, m_addr, sizeof(struct sockaddr_un)) == -1) {
        close();
        m_errStr = "bind(): ";
        m_errStr += strerror(m_err = errno);
        emit errorOccurred(m_err);
        return false;
    }

    if(::listen(m_sockfd, 0) == -1) {
        close();
        m_errStr = "listen(): ";
        m_errStr += strerror(m_err = errno);
        emit errorOccurred(m_err);
        return false;
    }

    if(!UnixSocket::setBlocking(m_sockfd, false)) {
        close();
        m_errStr = "UnixSocket::setBlocking(): ";
        m_errStr += strerror(m_err = errno);
        emit errorOccurred(m_err);
        return false;
    }

    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, [=]{ waitForNewConnection(0); });

    m_listening = true;
    return true;
}

UnixSocket* UnixSocketServer::nextPendingConnection() {
    if(m_pendingConnection.isEmpty())
        return nullptr;

    UnixSocket* sock = m_pendingConnection.dequeue();

    return sock;
}

void UnixSocketServer::close() {
    if(!m_listening)
        return;

    for(auto client : m_clients) {
        client->disconnect(this, nullptr);
        client->close();
        client->deleteLater();
    }
    m_clients.clear();

    if(m_readNotifier){
        m_readNotifier->disconnect();
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }

    if(m_addr) {
        if (unlink(((struct sockaddr_un*)m_addr)->sun_path) < 0 && errno != ENOENT) {
            fprintf(stderr,
                    "UnixSocketServer::close(): Failed to unlink socket: %s\n",
                    strerror(errno));
        }
        delete (struct sockaddr_un*)m_addr;
        m_addr = nullptr;
    }
    ::close(m_sockfd);
    m_sockfd = -1;
    m_listening = false;
}

bool UnixSocketServer::waitForNewConnection(int msec, bool *timedOut) {
    if(!m_listening)
        return false;

    QDeadlineTimer deadline(msec);

    do{
        int fd = ::accept(m_sockfd, NULL, NULL);
        
        if(fd == -1 && errno != EAGAIN){
            if(timedOut)
                *timedOut = false;
            m_err = errno;
            m_errStr = "accept(): ";
            m_errStr += strerror(m_err);
            emit errorOccurred(m_err);
            return false;
        }

        UnixSocket *sock = new UnixSocket(this);
        if(!sock->setFd(fd)){
            m_err = sock->error();
            m_errStr = "UnixSocket::setFd(): " + sock->errorString();
            ::close(fd);
            emit errorOccurred(m_err);
            sock->deleteLater();
            if(timedOut)
                *timedOut = false;
            return false;
        }

        auto removeSock = [this, sock] {
            sock->disconnect(this, nullptr);
            m_clients.remove(sock);
            if(m_pendingConnection.removeAll(sock)) {
                sock->deleteLater();
            }
        };

        connect(sock, &UnixSocket::destroyed, this, removeSock);
        connect(sock, &UnixSocket::disconnected, this, removeSock);

        m_clients.insert(sock);
        m_pendingConnection.enqueue(sock);

        emit newConnection();
            
        if(timedOut)
            *timedOut = false;
        return true;
    } while(!deadline.hasExpired()); 

    if(timedOut)
        *timedOut = deadline.hasExpired();
    return false;
}
