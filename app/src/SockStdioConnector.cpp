#include "SockStdioConnector.hpp"

#include <QtCore/QThread>

#include <termios.h>

SockStdioConnectorApp* SockStdioConnectorApp::m_instance = nullptr;

int main(int argc, char* argv[]) {
    struct termios term;
    tcgetattr(fileno(stdin), &term);

    cfmakeraw(&term);
    
    tcsetattr(fileno(stdin), 0, &term);

    SockStdioConnectorApp* app = SockStdioConnectorApp::Instance(argc, argv);
    if(app == nullptr){
        exit(1);
    }

    int ret = app->Exec();
    SockStdioConnectorApp::CleanUp();
    return ret;
}

SockStdioConnectorApp* SockStdioConnectorApp::Instance(int &argc, char* argv[]){
    assert(m_instance == nullptr);

    if(argc != 2){
        QTextStream(stderr) << "Usage: " << argv[0] << " [QLocalServer name]" << '\n';
        return nullptr;
    }

    m_instance = new SockStdioConnectorApp(argc, argv);
    m_instance->m_socketName = argv[1];

    return m_instance;
}

SockStdioConnectorApp* SockStdioConnectorApp::Instance(){
    assert(m_instance);
    
    return m_instance;
}

void SockStdioConnectorApp::CleanUp() {
    delete m_instance->m_socket;
    m_instance->m_socket = nullptr;

    delete m_instance;
    m_instance = nullptr;
}

void SockStdioConnectorApp::readStdin() {
    char c = getchar();;
    while(m_instance && m_instance->m_socket &&  m_instance->m_socket->isOpen()){
        emit m_instance->writeToSock(c);
        c = getchar();
    }
}

void SockStdioConnectorApp::writeToSockImpl(char c){
    m_socket->write(&c, 1);
    m_socket->flush();
}

void SockStdioConnectorApp::readSock(){
    QTextStream out(stdout);
    out << m_socket->readAll();
}

void SockStdioConnectorApp::printSocketError() {
    QTextStream(stderr) << "Socket error occurred: " << m_socket->errorString() << '\n';
    exit(1);
}

int SockStdioConnectorApp::Exec(){
    m_socket->connectToServer(m_socketName);
    if(m_socket->waitForConnected(1000) == false){
        QTextStream(stderr) << "Connecting to QLocalServer failed: " << m_socket->errorString() << '\n';
        return 1;
    }

    connect(this, SIGNAL(writeToSock(char)), this, SLOT(writeToSockImpl(char)));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readSock(void)));

    /* For some reason connecting via SIGNAL() for this signal does not work */
    connect(m_socket, &QLocalSocket::errorOccurred, this, &SockStdioConnectorApp::printSocketError);
    QThread::create(readStdin)->start();
    return QCoreApplication::exec();
}
