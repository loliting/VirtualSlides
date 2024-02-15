#include <QtCore/QCoreApplication>
#include <QtNetwork/QLocalSocket>

class SockStdioConnectorApp : public QCoreApplication {
    Q_OBJECT
public:
    static SockStdioConnectorApp* Instance();
    static SockStdioConnectorApp* Instance(int &argc, char* argv[]);
    
    static void CleanUp();

    int Exec();
signals:
    void writeToSock(char c);
private slots:
    void writeToSockImpl(char c);
    void readSock();
    void printSocketError(QLocalSocket::LocalSocketError);
private:
    SockStdioConnectorApp(int &argc, char *argv[]) : QCoreApplication(argc, argv) { }
    ~SockStdioConnectorApp() override { }

    static void readStdin();

    static SockStdioConnectorApp* m_instance;

    QString m_socketName;
    QLocalSocket* m_socket = new QLocalSocket();

};
