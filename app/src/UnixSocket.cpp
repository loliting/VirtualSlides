#include "UnixSocket.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

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
    memcpy(addr->sun_path, path.toUtf8().constData(), path.length());
    addr->sun_path[path.length()] = '\0';
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

    if(!setBlocking(false)) {
        close();
        return false;
    }

    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::readyRead);
    
    m_writeNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::handleWriteAvaliable);

    emit connected();

    return QIODevice::open(ReadWrite);
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

    if(m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if(m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        m_writeNotifier->deleteLater();
        m_writeNotifier = nullptr;
    }
    if(m_exceptionNotifier) {
        m_exceptionNotifier->setEnabled(false);
        m_exceptionNotifier->deleteLater();
        m_exceptionNotifier = nullptr;
    }
    if(m_addr) {
        delete m_addr;
        m_addr = nullptr;
    }
    
    QIODevice::close();

    emit disconnected();
}

bool UnixSocket::setFd(int fd) {
    m_sockfd = fd;

    if(!setBlocking(false))
        return false;

    m_readNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::readyRead);
    
    m_writeNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::handleWriteAvaliable);

    m_exceptionNotifier = new QSocketNotifier(m_sockfd, QSocketNotifier::Exception, this);
    connect(m_exceptionNotifier, &QSocketNotifier::activated,
        this, &UnixSocket::handleSocketException);

    return QIODevice::open(QIODevice::ReadWrite);
}

qint64 UnixSocket::readData(char *data, qint64 maxSize) {
    if(!isOpen())
        return -1;
    qint64 rc = ::read(m_sockfd, data, maxSize);
    if(rc < 0) {
        if(errno == EPIPE || errno == ECONNRESET) {
            close();
            return rc;
        }
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
    }

    return rc;
}

qint64 UnixSocket::writeData(const char *data, qint64 maxSize) {
    if(!isOpen())
        return -1;
    
    m_writeBuffor += QByteArray(data, maxSize);

    handleWriteAvaliable();
    if(m_writeNotifier)
        m_writeNotifier->setEnabled(!m_writeBuffor.isEmpty());

    return maxSize;
}

bool UnixSocket::handleWriteAvaliable() {
    if(m_writeBuffor.isEmpty()) {
        m_writeNotifier->setEnabled(false);
        return true;
    }
    
    errno = 0;
    ssize_t writtenBytes = ::write(m_sockfd, m_writeBuffor.data(), m_writeBuffor.size());
    
    if(writtenBytes > 0)
        m_writeBuffor.remove(0, writtenBytes);
    
    if(errno != EAGAIN && errno != 0) {
        close();
        if(errno == EPIPE || errno == ECONNRESET)
            return false;
        m_err = errno;
        setErrorString(strerror(m_err));
        emit errorOccurred(m_err);
    }

    emit bytesWritten(writtenBytes);
    
    return true;
}

qint64 UnixSocket::bytesAvailable() const {
    int result;
    ioctl(m_sockfd, FIONREAD, &result);
    return result + QIODevice::bytesAvailable();
}

qint64 UnixSocket::bytesToWrite() const {
    return m_writeBuffor.size() + QIODevice::bytesToWrite();
}

bool UnixSocket::canReadLine() const {
    assert(false && "UnixSocket::canReadLine() is not implemented");
    return 
        // m_readBuffor.contains('\n') ||
           QIODevice::canReadLine();
}

bool UnixSocket::waitForReadyRead(int msecs) {
    QDeadlineTimer deadline(msecs);

    qint64 bytesRead;

    do {
        bytesRead = bytesAvailable();
    } while(isOpen() & bytesRead == 0 && deadline.remainingTime());

    return bytesRead > 0;
}

bool UnixSocket::waitForBytesWritten(int msecs) {
    if(m_writeBuffor.isEmpty())
       return true;
    
    QDeadlineTimer deadline(msecs);

    do{
        if(!handleWriteAvaliable())
            return false;
    } while(isOpen() && m_writeBuffor.size() > 0 && deadline.remainingTime());

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

void UnixSocket::handleSocketException() {
    if(!isOpen())
        return;
    
    int err = 0;
    socklen_t optlen;
    
    if(getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &err, &optlen) == -1) {
        qWarning("An error occurred during getting socket error. %s", strerror(errno));
        return;
    }

    if(errno == EPIPE || errno == ECONNRESET) {
        close();
        return;
    }

    m_err = err;
    setErrorString(strerror(m_err));
    emit errorOccurred(m_err);
}
