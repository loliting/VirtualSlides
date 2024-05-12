#ifndef VSOCK_HPP
#define VSOCK_HPP

#include <QtCore/QtCore>
#include <QtCore/QSocketNotifier>

struct sockaddr;
class VSockServer;

class VSock : public QIODevice {
    Q_OBJECT
public:
    enum SpecialCIDs {
        Any = -1U,
        Hypervisor = 0,
        Local,
        Host
    };

    VSock(QObject *parent = nullptr) : QIODevice(parent) { }
    ~VSock();

    bool connectToHost(uint32_t cid, uint32_t port);
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
    static bool setBlocking(int fd, bool block);
    bool setBlocking(bool block);

    bool setFd(int fd);
    qint64 handleReadAvaliable();
    bool handleWriteAvaliable();
    void handleSocketException();
private:
    bool m_connected = false;
    int m_sockfd;
    struct sockaddr* m_addr = nullptr;

    int m_err = 0;

    QByteArray m_writeBuffor;
    QByteArray m_readBuffor;

    QSocketNotifier* m_readNotifier = nullptr;
    QSocketNotifier* m_writeNotifier = nullptr;
    QSocketNotifier* m_exceptionNotifier = nullptr;


    friend class VSockServer;
};

#endif // VSOCK_HPP