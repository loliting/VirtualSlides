#ifndef VSOCKUSERSERVER_HPP
#define VSOCKUSERSERVER_HPP

#include <QtCore/QObject>
#include <QtCore/QQueue>

class VSockUser;
class UnixSocketServer;

class VSockUserServer : public QObject {
    Q_OBJECT
public:
    enum SpecialCIDs {
        Any = -1U,
        Hypervisor = 0,
        Local,
        Host
    };

    VSockUserServer(QObject *parent = nullptr) : QObject(parent) { }
    ~VSockUserServer() { close(); }

    bool listen(const QString &path, uint32_t cid, uint32_t port);
    void close();
    
    bool hasPendingConnections() const { return !m_pendingConnection.isEmpty(); }
    VSockUser* nextPendingConnection() { return m_pendingConnection.isEmpty() ? nullptr : m_pendingConnection.dequeue(); }

    bool isListening() const;
    inline uint32_t cid() const { return m_cid; }
    inline uint32_t port() const { return m_port; }
    QString serverName() const;
    QString fullServerName() const;

    QString errorString() const { return m_errStr; }
    int error() const { return m_err; }
signals:
    void errorOccurred(int error);
    void newConnection();
private:
    UnixSocketServer* m_server = nullptr;

    bool m_listening = false;

    int m_err = 0;
    QString m_errStr = nullptr;

    uint32_t m_cid = 0;
    uint32_t m_port = 0;
    
    QQueue<VSockUser*> m_pendingConnection = QQueue<VSockUser*>();
};

#endif // VSOCKUSERSERVER_HPP