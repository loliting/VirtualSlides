#include "SockStdioConnector.hpp"

#include <QtCore/QThread>

#include <termios.h>

SockStdioConnectorApp* SockStdioConnectorApp::m_instance = nullptr;

#define IS_UTF8_2_CHAR(c) ((c) & '\xE0') == '\xC0' // UTF8-2 - 110xxxxx
#define IS_UTF8_3_CHAR(c) ((c) & '\xF0') == '\xE0' // UTF8-3 - 1110xxxx
#define IS_UTF8_4_CHAR(c) ((c) & '\xF8') == '\xF0' // UTF8-4 - 11110xxx


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
    while(m_instance->m_socket->bytesAvailable()){
        m_instance->charBuffor += m_socket->read(1);

        if(m_instance->expectedCharSize);
        else if(IS_UTF8_2_CHAR(m_instance->charBuffor[0]))
            m_instance->expectedCharSize = 2;
        else if(IS_UTF8_3_CHAR(m_instance->charBuffor[0]))
            m_instance->expectedCharSize = 3;
        else if(IS_UTF8_4_CHAR(m_instance->charBuffor[0]))
            m_instance->expectedCharSize = 4;
        else
            m_instance->expectedCharSize = 1;

        if(m_instance->charBuffor.size() >= m_instance->expectedCharSize){
            out << m_instance->charBuffor;
            m_instance->charBuffor.clear();
            m_instance->expectedCharSize = 0;
        }
    }
}

void SockStdioConnectorApp::printSocketError(QLocalSocket::LocalSocketError e) {
    if(e == QLocalSocket::LocalSocketError::PeerClosedError){
        QTextStream(stdout).flush();
        exit(0);
    }
    else {
        QTextStream(stdout).flush();
        QTextStream(stderr) << "\n" << m_socket->errorString();
        exit(1);
    }
}

int SockStdioConnectorApp::Exec(){
    m_socket->connectToServer(m_socketName);
    if(m_socket->waitForConnected(1000) == false){
        QTextStream(stderr) << "Connecting to QLocalServer failed: " << m_socket->errorString() << '\n';
        return 1;
    }

    connect(this, SIGNAL(writeToSock(char)), this, SLOT(writeToSockImpl(char)));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readSock(void)));
    connect(m_socket, SIGNAL(errorOccurred(QLocalSocket::LocalSocketError)), this, SLOT(printSocketError(QLocalSocket::LocalSocketError)));
    QThread::create(readStdin)->start();
    return QCoreApplication::exec();
}
