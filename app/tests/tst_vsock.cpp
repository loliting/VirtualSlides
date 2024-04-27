#include <QtTest/QtTest>

#include <QtCore/QThread>

#include "../src/VSock.hpp"
#include "../src/VSockServer.hpp"


#define CID  VSockServer::Local
#define PORT 100000

class tst_VSock : public QObject
{
    Q_OBJECT
private slots:
    void testGeneral();
    void testClose();
    void testEchoServer();
};

void tst_VSock::testClose() {
    VSockServer server1;
    VSockServer server2;

    QSignalSpy spyErrorOccurred1(&server1, &VSockServer::errorOccurred);
    QSignalSpy spyErrorOccurred2(&server2, &VSockServer::errorOccurred);

    server1.listen(CID, PORT);
    QVERIFY2(server1.isListening(), qUtf8Printable(server1.errorString()));
    server1.close();

    server2.listen(CID, PORT);
    QVERIFY2(server2.isListening(), qUtf8Printable(server2.errorString()));
    server2.close();
    
    QVERIFY2(spyErrorOccurred1.size() == 0, qUtf8Printable(server1.errorString()));
    QVERIFY2(spyErrorOccurred2.size() == 0, qUtf8Printable(server2.errorString()));
}

void tst_VSock::testGeneral() {
    VSockServer server;

    QSignalSpy spyNewConnection(&server, &VSockServer::newConnection);
    QSignalSpy spyErrorOccurred(&server, &VSockServer::errorOccurred);

    server.close();
    QCOMPARE(server.errorString().isEmpty(), true);
    QCOMPARE(server.hasPendingConnections(), false);
    QCOMPARE(server.isListening(), false);
    QCOMPARE(server.nextPendingConnection(), (VSock*)nullptr);
    QCOMPARE(server.listen(CID, PORT), true);

    QCOMPARE(spyNewConnection.size(), 0);
    QVERIFY2(spyErrorOccurred.size() == 0, qUtf8Printable(server.errorString()));
}

void tst_VSock::testEchoServer() {
    VSockServer server;
    QSignalSpy spy(&server, &VSockServer::newConnection);

    QVERIFY2(server.listen(CID, PORT), qUtf8Printable(server.errorString()));

    VSock socket;
    QSignalSpy spyConnected(&socket, &VSock::connected);
    QSignalSpy spyDisconnected(&socket, &VSock::disconnected);
    QSignalSpy spyError(&socket, &VSock::errorOccurred);
    QSignalSpy spyReadyRead(&socket, &VSock::readyRead);

    socket.connectToHost(CID, PORT);
    bool timedOut = true;
    int expectedReadyReadSignals = 0;

    server.waitForNewConnection(3000, &timedOut);

    QVERIFY(!timedOut);
    QCOMPARE(spyConnected.size(), 1);
    QVERIFY(socket.isOpen());

    if (server.hasPendingConnections()) {
        QString testLine = "test";
        for (int i = 0; i < 50000; ++i)
            testLine += QLatin1Char('a');
        VSock *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket);
        QVERIFY(serverSocket->isOpen());
        QTextStream out(serverSocket);
        QTextStream in(&socket);
        out << testLine << Qt::endl;
        bool wrote = serverSocket->waitForBytesWritten(3000);

        if (!socket.canReadLine()) {
            expectedReadyReadSignals = 1;
            QVERIFY(socket.waitForReadyRead(-1));
        }

        QVERIFY(socket.bytesAvailable() >= 0);
        QCOMPARE(socket.bytesToWrite(), (qint64)0);
        QCOMPARE(spyReadyRead.size(), expectedReadyReadSignals);

        QVERIFY(testLine.startsWith(in.readLine()));

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

QTEST_MAIN(tst_VSock)

#include "tst_vsock.moc"