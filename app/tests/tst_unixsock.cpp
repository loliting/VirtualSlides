#include <QtTest/QtTest>

#include <QtCore/QThread>
#include <QtCore/QUuid>

#include "../src/UnixSocket.hpp"
#include "../src/UnixSocketServer.hpp"

QString SERVER_PATH = QUuid::createUuid().toString();

class tst_UnixSocket : public QObject
{
    Q_OBJECT
private slots:
    void testGeneral();
    void testClose();
    void testEchoServer();
};

void tst_UnixSocket::testClose() {
    UnixSocketServer server1;
    UnixSocketServer server2;

    QSignalSpy spyErrorOccurred1(&server1, &UnixSocketServer::errorOccurred);
    QSignalSpy spyErrorOccurred2(&server2, &UnixSocketServer::errorOccurred);

    server1.listen(SERVER_PATH);
    QVERIFY2(server1.isListening(), qUtf8Printable(server1.errorString()));
    server1.close();

    server2.listen(SERVER_PATH);
    QVERIFY2(server2.isListening(), qUtf8Printable(server2.errorString()));
    server2.close();
    
    QVERIFY2(spyErrorOccurred1.size() == 0, qUtf8Printable(server1.errorString()));
    QVERIFY2(spyErrorOccurred2.size() == 0, qUtf8Printable(server2.errorString()));
}

void tst_UnixSocket::testGeneral() {
    UnixSocketServer server;

    QSignalSpy spyNewConnection(&server, &UnixSocketServer::newConnection);
    QSignalSpy spyErrorOccurred(&server, &UnixSocketServer::errorOccurred);

    server.close();
    QCOMPARE(server.errorString().isEmpty(), true);
    QCOMPARE(server.hasPendingConnections(), false);
    QCOMPARE(server.isListening(), false);
    QCOMPARE(server.nextPendingConnection(), (UnixSocket*)nullptr);
    QCOMPARE(server.listen(SERVER_PATH), true);

    QCOMPARE(spyNewConnection.size(), 0);
    QVERIFY2(spyErrorOccurred.size() == 0, qUtf8Printable(server.errorString()));
}

void tst_UnixSocket::testEchoServer() {
    UnixSocketServer server;
    QSignalSpy spy(&server, &UnixSocketServer::newConnection);

    QVERIFY2(server.listen(SERVER_PATH), qUtf8Printable(server.errorString()));

    UnixSocket socket;
    QSignalSpy spyConnected(&socket, &UnixSocket::connected);
    QSignalSpy spyDisconnected(&socket, &UnixSocket::disconnected);
    QSignalSpy spyError(&socket, &UnixSocket::errorOccurred);
    QSignalSpy spyReadyRead(&socket, &UnixSocket::readyRead);

    QVERIFY2(socket.connectToServer(server.fullServerName()), qUtf8Printable(socket.errorString()));
    bool timedOut = true;

    server.waitForNewConnection(3000, &timedOut);

    QVERIFY(!timedOut);
    QCOMPARE(spyConnected.size(), 1);
    QVERIFY(socket.isOpen());

    if (server.hasPendingConnections()) {
        QString testLine = "\'test";
        for (int i = 0; i < 50000; ++i)
            testLine += QLatin1Char('a' + i % 3);
        testLine += QLatin1Char('\'');
        UnixSocket *serverSocket = server.nextPendingConnection();
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
    }

    socket.close();
    QCOMPARE(spyConnected.size(), 1);
    QCOMPARE(spyDisconnected.size(), 1);
    QCOMPARE(spyError.size(), 0);

    server.close();
}

QTEST_MAIN(tst_UnixSocket)

#include "tst_unixsock.moc"