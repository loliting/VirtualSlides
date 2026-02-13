#ifndef UNIXSOCKETSERVER_HPP
#define UNIXSOCKETSERVER_HPP

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QQueue>
#include <QtCore/QSocketNotifier>

class UnixSocket;

class UnixSocketServer : public QObject {
    Q_OBJECT
public:
    UnixSocketServer(QObject *parent = nullptr);
    ~UnixSocketServer();

    bool listen(QString path);
    void close();
    
    bool waitForNewConnection(int msec = 0, bool *timedOut = nullptr);

    bool hasPendingConnections() const { return !m_pendingConnection.isEmpty(); }
    UnixSocket* nextPendingConnection();

    bool isListening() const { return m_listening; }
    QString serverName() const { return m_serverName; }
    QString fullServerName() const { return m_serverPath; }

    QString errorString() const { return m_errStr; }
    int error() const { return m_err; }
signals:
    void errorOccurred(int error);
    void newConnection();
private:
    bool m_listening = false;

    int m_err = 0;
    QString m_errStr = nullptr;

    QString m_serverPath = nullptr;
    QString m_serverName = nullptr;
    
    struct sockaddr* m_addr = nullptr;
    int m_sockfd = -1;
    QSocketNotifier* m_readNotifier = nullptr;

    QSet<UnixSocket*> m_clients = QSet<UnixSocket*>();
    QQueue<UnixSocket*> m_pendingConnection = QQueue<UnixSocket*>();
};

#endif // UNIXSOCKETSERVER_HPP