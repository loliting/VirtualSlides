#include <QtCore/QCoreApplication>

#include "UnixSocket.hpp"

class SockStdioConnectorApp : public QCoreApplication {
    Q_OBJECT
public:
    static SockStdioConnectorApp* Instance();
    static SockStdioConnectorApp* Instance(int &argc, char* argv[]);
    
    static void CleanUp();

    int exec();
signals:
    void writeToSock(char c);
private slots:
    void writeToSockImpl(char c);
    void readSock();
    void printSocketError(int error);
private:
    SockStdioConnectorApp(int &argc, char *argv[]) : QCoreApplication(argc, argv) { }
    ~SockStdioConnectorApp() override { }

    static void readStdin();

    static SockStdioConnectorApp* m_instance;

    QString m_socketName;
    UnixSocket* m_socket = new UnixSocket(this);
    QThread* m_thread = nullptr;
};
