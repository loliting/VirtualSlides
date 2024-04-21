#ifndef VSOCKSERVER_HPP
#define VSOCKSERVER_HPP

#include <QtCore/QObject>
#include <QtCore/QSet>

#include "VSock.hpp"

class VSockServer : public QObject {
    Q_OBJECT
public:
    enum SpecialCIDs {
        Any = -1U,
        Hypervisor = 0,
        Local,
        Host
    };

    VSockServer(QObject *parent = nullptr);
    ~VSockServer();

    bool listen(uint32_t cid, uint32_t port);
    void close();
    
    bool hasPendingConnections() const { return !m_pendingConnection.isEmpty(); }
    VSock* nextPendingConnection();

    bool isListening() const { return m_listening; }
    uint32_t cid() const { return m_cid; }
    uint32_t port() const { return m_port; }

    QString errorString() const { return m_errStr; }
    int error() const { return m_err; }
signals:
    void errorOccurred(int error);
    void newConnection();
private slots:
    void handleClientAvaliable();
private:
    bool m_listening = false;

    int m_err = 0;
    QString m_errStr = nullptr;

    uint32_t m_cid = 0;
    uint32_t m_port = 0;
    
    struct sockaddr* m_addr = nullptr;
    int m_sockfd = -1;
    QSocketNotifier* m_readNotifier = nullptr;

    QSet<VSock*> m_clients = QSet<VSock*>();
    QQueue<VSock*> m_pendingConnection = QQueue<VSock*>();
};

#endif // VSOCKSERVER_HPP