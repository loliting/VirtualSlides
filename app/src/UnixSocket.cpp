#include "UnixSocket.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

int UnixSocket::makeSocket(const QString& path, struct sockaddr** addrp) {
    int sockfd = -1;

    int type = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
    type |= SOCK_CLOEXEC;
#endif
    if((sockfd = socket(AF_UNIX, type, 0)) == -1) {
        return sockfd;
    }

    const int reuseAddrEnabled = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseAddrEnabled, sizeof(int)) < 0) {
        ::close(sockfd);
        return -1;
    }

    if(!setBlocking(sockfd, true)) {
        ::close(sockfd);
        return -1;
    }

    struct sockaddr_un *addr = new struct sockaddr_un;
    addr->sun_family = AF_UNIX;
    addr->sun_path[0] = '\0';
    QByteArray utf8Path = path.toUtf8();
    memcpy(addr->sun_path, utf8Path.constData(), utf8Path.size());
    addr->sun_path[utf8Path.size()] = '\0';
    *addrp = (struct sockaddr*)addr;
    
    return sockfd;
}

bool UnixSocket::connectToServer(const QString& path) {
    if(isOpen()) {
        m_err = EPERM;
        setErrorString("Socket already connected");
        return false;
    }
    
    if((m_sockfd = makeSocket(path, &m_addr)) == -1) {
        m_err = errno;
        setErrorString(strerror(m_err));
        return false;
    }

    if(::connect(m_sockfd, m_addr, sizeof(struct sockaddr_un)) == -1) {
        m_err = errno;
        setErrorString(strerror(m_err));
        close();
        return false;
    }

    return setFd(m_sockfd);
}

UnixSocket::~UnixSocket() {
    if(isOpen())
        close();
}

void UnixSocket::close() {
    if(!isOpen())
        return;
        
    ::close(m_sockfd);
    m_sockfd = -1;

    m_readNotifier.setEnabled(false);
    m_readNotifier.setSocket(-1);
    m_writeNotifier.setEnabled(false);
    m_writeNotifier.setSocket(-1);

    if(m_addr) {
        delete (struct sockaddr_un*)m_addr;
        m_addr = nullptr;
    }
    
    QIODevice::close();

    emit disconnected();
}

bool UnixSocket::setFd(int fd) {
    m_sockfd = fd;

    if(!setBlocking(false)) {
        close();
        return false;
    }

    m_readNotifier.setSocket(m_sockfd);
    connect(&m_readNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::handleReadAvaliable);
    m_readNotifier.setEnabled(true);
    
    m_writeNotifier.setSocket(m_sockfd);
    connect(&m_writeNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::handleWriteAvaliable);
    m_writeNotifier.setEnabled(false);

    emit connected();

    return QIODevice::open(QIODevice::ReadWrite);
}

qint64 UnixSocket::readData(char *data, qint64 maxSize) {
    if(!isOpen())
        return -1;
    qint64 size = qMin<qint64>(maxSize, m_readBuffer.size());
    memmove(data, m_readBuffer.constData(), size);
    m_readBuffer.remove(0, size);
    return size;
}

qint64 UnixSocket::writeData(const char *data, qint64 maxSize) {
    if(!isOpen())
        return -1;
    
    m_writeBuffer += QByteArray(data, maxSize);

    handleWriteAvaliable();
    m_writeNotifier.setEnabled(!m_writeBuffer.isEmpty());

    return maxSize;
}

void UnixSocket::handleReadAvaliable() {
    char cbuf[8] = { 0 };
    
    qint64 bytesRead = 0;
    qint64 tmpBytesRead = 0;

    errno = 0;
    do {
        tmpBytesRead = ::recv(m_sockfd, cbuf, 8, 0);
        if(errno == EAGAIN) break;
        if(tmpBytesRead < 0) {
            handleSocketException(errno);
            break;
        }
        else if (tmpBytesRead == 0) {
            close();
            break;
        }
        m_readBuffer.append(cbuf, tmpBytesRead);
        bytesRead += tmpBytesRead;
    } while(tmpBytesRead > 0);

    if(bytesRead > 0) {
        emit readyRead();
    }
}

bool UnixSocket::handleWriteAvaliable() {
    if(m_writeBuffer.isEmpty()) {
        m_writeNotifier.setEnabled(false);
        return true;
    }
    
    ssize_t writtenBytes = ::write(m_sockfd, m_writeBuffer.data(), m_writeBuffer.size());
    
    if(writtenBytes > 0) {
        m_writeBuffer.remove(0, writtenBytes);
        emit bytesWritten(writtenBytes);
        return true;
    }
    else if(writtenBytes < 0) {
        handleSocketException(errno);
    }
    return false;
}

qint64 UnixSocket::bytesAvailable() const {
    return m_readBuffer.size() + QIODevice::bytesAvailable();
}

qint64 UnixSocket::bytesToWrite() const {
    return m_writeBuffer.size() + QIODevice::bytesToWrite();
}

bool UnixSocket::canReadLine() const {
    return m_readBuffer.contains('\n') || QIODevice::canReadLine();
}

bool UnixSocket::waitForReadyRead(int msecs) {
    QDeadlineTimer deadline(msecs);

    qint64 bytesRead;

    do {
        handleReadAvaliable();
        bytesRead = bytesAvailable();
    } while(isOpen() && bytesRead == 0 && deadline.remainingTime());

    return bytesRead > 0;
}

bool UnixSocket::waitForBytesWritten(int msecs) {
    if(m_writeBuffer.isEmpty())
       return true;
    
    QDeadlineTimer deadline(msecs);

    do{
        if(!handleWriteAvaliable())
            return false;
    } while(isOpen() && m_writeBuffer.size() > 0 && deadline.remainingTime());

    return true;
}

bool UnixSocket::isBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) {
        fprintf(stderr,
                "UnixSocket::isBlocking(): fcntl() failed: %s. "
                "Assuming blocking socket\n",
                strerror(errno));
        return true;
    }
    return !(flags & O_NONBLOCK);
}

bool UnixSocket::isBlocking() {
    return isBlocking(m_sockfd);
}

bool UnixSocket::setBlocking(int fd, bool block) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    flags |= !block * O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags) >= 0;
}

bool UnixSocket::setBlocking(bool block) {
    if(!setBlocking(m_sockfd, block)) {
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
    }
    return true;
}

void UnixSocket::handleSocketException(int error) {
    if(error == 0 || error == EAGAIN)  {
        return;
    }

    if(error == EPIPE || error == ECONNRESET) {
        close();
        return;
    }

    setErrorString(strerror(m_err = error));
    emit errorOccurred(m_err);
}