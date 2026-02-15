#include "SockStdioConnector.hpp"

#include <QtCore/QThread>
#include <csignal>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#error "This platform is not supported"
#endif

SockStdioConnectorApp* SockStdioConnectorApp::m_instance = nullptr;

#define IS_UTF8_2_CHAR(c) ((c) & '\xE0') == '\xC0' // UTF8-2 - 110xxxxx
#define IS_UTF8_3_CHAR(c) ((c) & '\xF0') == '\xE0' // UTF8-3 - 1110xxxx
#define IS_UTF8_4_CHAR(c) ((c) & '\xF8') == '\xF0' // UTF8-4 - 11110xxx


int main(int argc, char* argv[]) {
#ifdef HAVE_TERMIOS_H
    struct termios term;
    tcgetattr(fileno(stdin), &term);
    cfmakeraw(&term);
    tcsetattr(fileno(stdin), 0, &term);
#else
#error "This platform is not supported"
#endif

    SockStdioConnectorApp* app = SockStdioConnectorApp::Instance(argc, argv);
    if(app == nullptr){
        exit(1);
    }

    int ret = app->exec();
    SockStdioConnectorApp::CleanUp();
    return ret;
}

SockStdioConnectorApp* SockStdioConnectorApp::Instance(int &argc, char* argv[]){
    assert(m_instance == nullptr);

    if(argc != 2){
        QTextStream(stderr) << "Usage: " << argv[0] << " [UDS path]" << '\n';
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
    m_instance->m_socket->deleteLater();
    m_instance->m_socket = nullptr;

    m_instance->deleteLater();
    m_instance = nullptr;
}

void SockStdioConnectorApp::readStdin() {
    do{
        emit m_instance->writeToSock(getchar());
    } while(m_instance && m_instance->m_socket && m_instance->m_socket->isOpen());
}

void SockStdioConnectorApp::writeToSockImpl(char c){
    m_socket->write(&c, 1);
}

void SockStdioConnectorApp::readSock() {
    static char buf[4];
    static off_t offset = 0;
    static uint8_t expectedCharSize = 0;
    while(m_instance->m_socket->bytesAvailable() > 0) {
        qint64 rc = m_socket->readData(buf + offset, 1);
        if(rc > 0) {
            offset += rc;
        }
        else if(rc < 0) {
            printSocketError(-rc);
        }
        else {
            continue;
        }

        // TODO: sometimes characters still display as ? signs
        if(expectedCharSize);
        else if(IS_UTF8_2_CHAR(buf[0])) {
            expectedCharSize = 2;
        }
        else if(IS_UTF8_3_CHAR(buf[0])) {
            expectedCharSize = 3;
        }
        else if(IS_UTF8_4_CHAR(buf[0])) {
            expectedCharSize = 4;
        }
        else {
            expectedCharSize = 1;
        }

        if(offset >= expectedCharSize) {
            printf("%.*s", (int)offset, buf);
            fflush(stdout);
            expectedCharSize = 0;
            offset = 0;
        }
    }
}

void SockStdioConnectorApp::printSocketError(int err) {
    QTextStream(stdout).flush();
    QTextStream(stderr) << "\n" << m_socket->errorString();
    exit(1);
}

int SockStdioConnectorApp::exec() {
    if(m_socket->connectToServer(m_socketName) == false){
        QTextStream(stderr) << "Connecting to UDS \"" << m_socketName
            << "\" failed: " << m_socket->errorString() << '\n';
        return 1;
    }

    connect(this, &SockStdioConnectorApp::writeToSock, this, &SockStdioConnectorApp::writeToSockImpl);
    connect(m_socket, &UnixSocket::readyRead, this, &SockStdioConnectorApp::readSock);
    connect(m_socket, &UnixSocket::disconnected, this, [this] {
        /* We have a blocking call in another thread and this 
         * is simplest way to enforce process termination */ 
        raise(SIGINT);
    }, Qt::QueuedConnection);
    connect(m_socket, &UnixSocket::errorOccurred, this, &SockStdioConnectorApp::printSocketError);
    m_thread = QThread::create(readStdin);
    m_thread->start();
    return QCoreApplication::exec();
}