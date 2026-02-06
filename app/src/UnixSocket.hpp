#ifndef UNIXSOCKET_HPP
#define UNIXSOCKET_HPP

#include <QtCore/QtCore>
#include <QtCore/QSocketNotifier>

struct sockaddr;
class UnixSocketServer;

class UnixSocket : public QIODevice {
    Q_OBJECT
public:
    UnixSocket(QObject *parent = nullptr) : QIODevice(parent) { }
    ~UnixSocket();

    bool connectToServer(const QString& path);
    void close() override;

    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;

    bool waitForReadyRead(int msecs) override;
    bool waitForBytesWritten(int msecs) override;
    
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

    int error() const { return m_err; }
signals:
    void connected();

    void disconnected(); 
    void errorOccurred(int error);
private:
    static int makeSocket(const QString& path, struct sockaddr** addrp);

    static bool setBlocking(int fd, bool block);
    bool setBlocking(bool block);

    static bool isBlocking(int fd);
    bool isBlocking();

    bool setFd(int fd);
    bool handleWriteAvaliable();
    void handleSocketException();
private:
    bool m_connected = false;
    int m_sockfd = -1;
    struct sockaddr* m_addr = nullptr;

    int m_err = 0;

    QByteArray m_writeBuffor;

    QSocketNotifier* m_readNotifier = nullptr;
    QSocketNotifier* m_writeNotifier = nullptr;
    QSocketNotifier* m_exceptionNotifier = nullptr;

    friend class UnixSocketServer;
};

#endif // VSOCK_HPP