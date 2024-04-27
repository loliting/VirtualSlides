#include "VSock.hpp"

#include <QtCore/qsystemdetection.h>

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

#ifdef Q_OS_LINUX
#include <linux/vm_sockets.h>
#elif defined(Q_OS_MACOS)
#include <sys/vsock.h>
#endif

bool VSock::connectToHost(uint32_t cid, uint32_t port) {
    if(m_connected){
        m_err = EPERM;
        setErrorString("Socket already connected");
        return false;
    }
    
    if((m_sockfd = socket(AF_VSOCK, SOCK_STREAM, 0)) == -1) {
        m_err = errno;
        setErrorString(strerror(m_err));
        return false;
    }

    struct sockaddr_vm *addr = new struct sockaddr_vm;
    addr->svm_cid = cid;
    addr->svm_family = AF_VSOCK;
    addr->svm_port = port;
    m_addr = (struct sockaddr*)addr;

    if(::connect(m_sockfd, m_addr, sizeof(struct sockaddr_vm)) == -1) {
        m_err = errno;
        setErrorString(strerror(m_err));
        return false;
    }

    if(!setBlocking(false))
        return false;

    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, &VSock::handleReadAvaliable);
    
    m_writeNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated,
        this, &VSock::handleWriteAvaliable);

    m_connected = QIODevice::open(ReadWrite);
    if(!m_connected)
        close();
    
    emit connected();

    return m_connected;
}

VSock::~VSock() {
    if(m_connected)
        close();
}

void VSock::close() {
    if(!m_connected)
        return;
    
    m_connected = false;
    ::close(m_sockfd);

    if(m_readNotifier){
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if(m_writeNotifier){
        m_writeNotifier->deleteLater();
        m_writeNotifier = nullptr;
    }
    if(m_addr){
        delete m_addr;
        m_addr = nullptr;
    }
    
    QIODevice::close();

    emit disconnected();
}

bool VSock::setFd(int fd) {
    m_sockfd = fd;
    m_connected = true;

    if(!setBlocking(false))
        return false;

    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, &VSock::handleReadAvaliable);
    
    m_writeNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated,
        this, &VSock::handleWriteAvaliable);

    m_connected = QIODevice::open(QIODevice::ReadWrite);
    return m_connected;
}

qint64 VSock::readData(char *data, qint64 maxSize) {
    if(!m_connected)
        return -1;

    qint64 size = qMin<qint64>(maxSize, m_readBuffor.size());
    memmove(data, m_readBuffor.data(), size);
    m_readBuffor.remove(0, size);

    return size;
}

qint64 VSock::writeData(const char *data, qint64 maxSize) {
    if(!m_connected)
        return -1;
    
    m_writeBuffor += QByteArray(data, maxSize);

    handleWriteAvaliable();
    m_writeNotifier->setEnabled(!m_writeBuffor.isEmpty());

    return maxSize;
}


qint64 VSock::handleReadAvaliable() {
    char c;
    errno = 0;
    
    qint64 bytesRead = 0;

    while (::read(m_sockfd, &c, 1) > 0) {
        m_readBuffor += c;
        ++bytesRead;
    }

    if(errno != EAGAIN && errno != 0) {
        if(errno == EPIPE){
            close();
            emit disconnected();
            return -1;
        }
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
        return -1;
    }

    if(bytesRead > 0)
        emit readyRead();

    return bytesRead;
}

bool VSock::handleWriteAvaliable() {
    if(m_writeBuffor.isEmpty()){
        m_writeNotifier->setEnabled(false);
        return true;
    }
    
    errno = 0;
    ssize_t writtenBytes = ::write(m_sockfd, m_writeBuffor.data(), m_writeBuffor.size());
    
    if(writtenBytes > 0)
        m_writeBuffor.remove(0, writtenBytes);
    
    if(errno != EAGAIN && errno != 0) {
        if(errno == EPIPE){
            close();
            emit disconnected();
            return false;
        }
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
    }

    emit bytesWritten(writtenBytes);
    
    return true;
}

qint64 VSock::bytesAvailable() const {
    return m_readBuffor.size() + QIODevice::bytesAvailable();
}

qint64 VSock::bytesToWrite() const {
    return m_writeBuffor.size() + QIODevice::bytesToWrite();
}

bool VSock::canReadLine() const {
    return m_readBuffor.contains('\n') || QIODevice::canReadLine();
}

bool VSock::waitForReadyRead(int msecs){
    if(!m_readBuffor.isEmpty())
       return true;
    
    QDeadlineTimer deadline(msecs);

    qint64 bytesRead;

    do{
        bytesRead = handleReadAvaliable(); 
    } while(bytesRead == 0 && deadline.remainingTime());

    return bytesRead > 0;
}

bool VSock::waitForBytesWritten(int msecs){
    if(m_writeBuffor.isEmpty())
       return true;
    
    QDeadlineTimer deadline(msecs);

    do{
        if(!handleWriteAvaliable())
            return false;
    } while(m_writeBuffor.size() > 0 && deadline.remainingTime());

    return true;
}

bool VSock::setBlocking(int fd, bool block){
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    flags |= (!block * O_NONBLOCK);

    return fcntl(fd, F_SETFL, flags) != -1;
}

bool VSock::setBlocking(bool block){
    if(!setBlocking(m_sockfd, block)){
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
    }
    return true;
}