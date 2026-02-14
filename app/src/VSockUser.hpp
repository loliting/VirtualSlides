#ifndef VSOCKUSER_HPP
#define VSOCKUSER_HPP

#include <QtCore/QtCore>
#include <QtCore/QSocketNotifier>

class UnixSocket;
class VSockUserServer;

class VSockUser : public QIODevice {
    Q_OBJECT
public:
    enum SpecialCIDs : uint32_t {
        Any = -1U,
        Hypervisor = 0,
        Local,
        Host
    };

    VSockUser(QObject *parent = nullptr) : QIODevice(parent) { }
    ~VSockUser();

    /* NOTE: this operation is blocking */
    bool connectToServer(QString serverPath, 
        uint32_t host_cid, uint32_t host_port,
        uint32_t vm_cid, uint32_t vm_port,
        int msec = 1000, bool* timeout = nullptr);
    void close() override;

    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;

    bool waitForReadyRead(int msecs) override;
    bool waitForBytesWritten(int msecs) override;
    
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

    /* Returns SpecialCIDs::Any on error */
    uint32_t hostCid() const;
    /* Returns SpecialCIDs::Any on error */
    uint32_t vmCid() const;
    /* Returns -1U on error */
    uint32_t hostPort() const;
    /* Returns -1U on error */
    uint32_t vmPort() const;

    int error() const { return m_err; }
signals:
    void connected();

    void disconnected(); 
    void errorOccurred(int error);
private:
    void setSock(UnixSocket* sock);
private:
    struct __attribute__((packed)) VSockUserConnectionKey {
        uint64_t host_cid;
        uint64_t vm_cid;
        uint32_t host_port;
        uint32_t vm_port;
    };
    enum ConnectionReply : uint8_t {
        Reject = 0,
        Accept = 1
    };
private:
    UnixSocket* m_sock = nullptr;
    VSockUserConnectionKey connectionData = { 0 };

    int m_err = 0;

    QByteArray m_writeBuffor;
    QByteArray m_readBuffor;

    friend class VSockUserServer;
};

#endif // VSOCKUSER_HPP