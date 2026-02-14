#include "VSockUser.hpp"

#include "UnixSocket.hpp"

void VSockUser::setSock(UnixSocket* sock) {
    sock->setParent(this);
    m_sock = sock;
    setProperty("_unixSocket", QVariant::fromValue<UnixSocket*>(sock));

    connect(m_sock, &UnixSocket::disconnected, this, &VSockUser::close);
    connect(m_sock, &UnixSocket::errorOccurred, this, [this]() {
        setErrorString(m_sock->errorString());
        emit errorOccurred(m_err = m_sock->error());
    });
    connect(m_sock, &UnixSocket::readyRead, this, &VSockUser::readyRead);
    connect(m_sock, &UnixSocket::bytesWritten, this, &VSockUser::bytesWritten);
}

bool VSockUser::connectToServer(QString serverPath, 
    uint32_t host_cid, uint32_t host_port,
    uint32_t vm_cid, uint32_t vm_port,
    int timeoutMsec, bool* timeout)
{
    UnixSocket* sock = new UnixSocket(this);
    if(!sock->connectToServer(serverPath)) {
        setErrorString("UnixSocket::connectToServer(): " + sock->errorString());
        emit errorOccurred(m_err = sock->error());
        return false;
    }

    connectionData = {
        .host_cid = host_cid,
        .vm_cid = vm_cid,
        .host_port = host_port,
        .vm_port = vm_port
    };
    sock->writeData((const char*)&connectionData, sizeof(connectionData)); 
    VSockUser::ConnectionReply reply;
    QDeadlineTimer deadline(timeoutMsec);
    while(!deadline.hasExpired() && sock->bytesAvailable() < 1) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
    }
    if(sock->readData((char*)&reply, sizeof(reply)) < 1) {
        setErrorString("UnixSocket::readData(): " + sock->errorString());
        emit errorOccurred(m_err = sock->error());
        close();
        return false;
    }
    if(reply != VSockUser::ConnectionReply::Accept) {
        m_err = ECONNREFUSED;
        setErrorString(QString(strerror(m_err)));
        emit errorOccurred(m_err);
        close();
        return false;
    }

    setSock(sock);

    emit connected();
    return QIODevice::open(ReadWrite);
}

VSockUser::~VSockUser() {
    close();
}

void VSockUser::close() {
    if(m_sock) {
        setProperty("_unixSocket", QVariant::fromValue<UnixSocket*>(nullptr));
        m_sock->deleteLater();
        m_sock = nullptr;
    }

    QIODevice::close();
    emit disconnected();
}

qint64 VSockUser::bytesAvailable() const {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return 0;
    }
    return m_sock->bytesAvailable();
}

qint64 VSockUser::bytesToWrite() const {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return 0;
    }
    return m_sock->bytesToWrite();
}

bool VSockUser::canReadLine() const {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return false;
    }
    return m_sock->canReadLine();
}

bool VSockUser::waitForReadyRead(int msecs) {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return false;
    }
    return m_sock->waitForReadyRead(msecs);
}

bool VSockUser::waitForBytesWritten(int msecs) {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return false;
    }
    return m_sock->waitForBytesWritten(msecs);
}

qint64 VSockUser::readData(char *data, qint64 maxSize) {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return -1;
    }
    return m_sock->readData(data, maxSize);
}

qint64 VSockUser::writeData(const char *data, qint64 maxSize) {
    if(!m_sock && !m_sock->isOpen() && !isOpen()) {
        return -1;
    }
    return m_sock->writeData(data, maxSize);
}

uint32_t VSockUser::hostCid() const {
    if(m_sock && m_sock->isOpen())
        return connectionData.host_cid;
    return SpecialCIDs::Any;
}

uint32_t VSockUser::vmCid() const {
    if(m_sock && m_sock->isOpen())
        return connectionData.vm_cid;
    return -1U;
}

uint32_t VSockUser::hostPort() const {
    if(m_sock && m_sock->isOpen())
        return connectionData.host_port;
    return SpecialCIDs::Any;
}

uint32_t VSockUser::vmPort() const {
    if(m_sock && m_sock->isOpen())
        return connectionData.vm_port;
    return -1U;
}