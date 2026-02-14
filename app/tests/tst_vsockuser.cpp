#include <QtTest/QtTest>

#include <QtCore/QThread>

#include <chrono>

#include "../src/UnixSocket.hpp"
#include "../src/VSockUser.hpp"
#include "../src/VSockUserServer.hpp"

using namespace std::chrono;

QString SERVER_PATH = QUuid::createUuid().toString();

#define SERVER_CID  VSockUserServer::Local
#define SERVER_PORT 100000
#define CLIENT_CID  VSockUserServer::Local
#define CLIENT_PORT ((-2U))

class tst_VSockUser : public QObject
{
    Q_OBJECT
private slots:
    void testGeneral();
    void testClose();
    void testEchoServer();
};

void tst_VSockUser::testClose() {
    VSockUserServer server1;
    VSockUserServer server2;

    QSignalSpy spyErrorOccurred1(&server1, &VSockUserServer::errorOccurred);
    QSignalSpy spyErrorOccurred2(&server2, &VSockUserServer::errorOccurred);

    server1.listen(SERVER_PATH, SERVER_CID, SERVER_PORT);
    QVERIFY2(server1.isListening(), qUtf8Printable(server1.errorString()));
    QString path = server1.fullServerName();
    server1.close();

    QVERIFY(!QFileInfo(path).exists());

    server2.listen(SERVER_PATH, SERVER_CID, SERVER_PORT);
    QVERIFY2(server2.isListening(), qUtf8Printable(server2.errorString()));
    QCOMPARE(path, server2.fullServerName());
    server2.close();
    
    QVERIFY2(spyErrorOccurred1.size() == 0, qUtf8Printable(server1.errorString()));
    QVERIFY2(spyErrorOccurred2.size() == 0, qUtf8Printable(server2.errorString()));
}

void tst_VSockUser::testGeneral() {
    VSockUserServer server;

    QSignalSpy spyNewConnection(&server, &VSockUserServer::newConnection);
    QSignalSpy spyErrorOccurred(&server, &VSockUserServer::errorOccurred);

    server.close();
    QCOMPARE(server.errorString().isEmpty(), true);
    QCOMPARE(server.hasPendingConnections(), false);
    QCOMPARE(server.isListening(), false);
    QCOMPARE(server.nextPendingConnection(), (VSockUser*)nullptr);
    QCOMPARE(server.listen(SERVER_PATH, SERVER_CID, SERVER_PORT), true);

    QCOMPARE(spyNewConnection.size(), 0);
    QVERIFY2(spyErrorOccurred.size() == 0, qUtf8Printable(server.errorString()));
}

void tst_VSockUser::testEchoServer() {
    VSockUserServer server;
    QSignalSpy spy(&server, &VSockUserServer::newConnection);

    QVERIFY2(server.listen(SERVER_PATH, SERVER_CID, SERVER_PORT), qUtf8Printable(server.errorString()));

    VSockUser socket;
    QSignalSpy spyConnected(&socket, &VSockUser::connected);
    QSignalSpy spyDisconnected(&socket, &VSockUser::disconnected);
    QSignalSpy spyError(&socket, &VSockUser::errorOccurred);
    QSignalSpy spyReadyRead(&socket, &VSockUser::readyRead);

    QCOMPARE(spyConnected.size(), 0);

    QVERIFY2(socket.connectToServer(server.fullServerName(),
        SERVER_CID, SERVER_PORT,
        CLIENT_CID, CLIENT_PORT
    ), qUtf8Printable(socket.errorString()));
    QVERIFY(spyConnected.size() == 1 || spyConnected.wait(3000));
    QVERIFY(socket.isOpen());

    if (server.hasPendingConnections()) {
        QString testLine = "\'test";
        for (int i = 0; i < 50000; ++i)
            testLine += QLatin1Char('a' + i % 3);
        testLine += QLatin1Char('\'');
        VSockUser* serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket);
        QVERIFY(serverSocket->isOpen());
        QTextStream out(serverSocket);
        QTextStream in(&socket);
        out << testLine << Qt::endl;
        out << testLine;
        bool wrote = serverSocket->waitForBytesWritten(3000);

        if (!socket.canReadLine()) {
            QVERIFY(socket.waitForReadyRead(3000));
        }

        QVERIFY(socket.bytesAvailable() >= 0);
        QCOMPARE(socket.bytesToWrite(), (qint64)0);
        
        QVERIFY(spyReadyRead.size() >= 1 || spyReadyRead.wait(1000));
        QVERIFY(spyReadyRead.size() >= 1);

        QVERIFY(testLine == in.readLine());
        QCOMPARE(testLine, in.readAll());

        QVERIFY2(wrote || serverSocket->waitForBytesWritten(1000), qUtf8Printable(serverSocket->errorString()));

        QCOMPARE(serverSocket->errorString(), QString("Unknown error"));
        QCOMPARE(socket.errorString(), QString("Unknown error"));
        serverSocket->close();
    }

    QCOMPARE(spyConnected.size(), 1);
    QVERIFY(spyDisconnected.size() == 1 || spyDisconnected.wait(1000));
    QCOMPARE(spyError.size(), 0);

    server.close();
}

QTEST_MAIN(tst_VSockUser)

#include "tst_vsockuser.moc"