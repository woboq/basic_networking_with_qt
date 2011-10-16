// This code is released as public domain.
// -- Markus Goetz
#include <QtGui/QApplication>
#include "qmlapplicationviewer.h"
#include <QtNetwork/QUdpSocket>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QDeclarativeContext>
#include <QAbstractTableModel>
#include <QMap>
#include <QXmlQuery>

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
class NodeList : public QAbstractListModel {
    Q_OBJECT
public:
    enum NodeDataRoles {
        AddrRole = Qt::UserRole + 1,
        ChatLogRole
    };

    NodeList()  {
        QHash<int, QByteArray> roles;
        roles[AddrRole] = "addr";
        roles[ChatLogRole] = "log";
        setRoleNames(roles);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const {
        return nodes.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const {
        return 1;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const {
        QMap<QString,QString>::const_iterator i = nodes.begin();
        i += index.row();
        if (role == AddrRole)
            return i.key();
        else if (role == ChatLogRole)
            return i.value();
        else
            return "";
    }


public slots:
    void nodeDiscoveredSlot(QHostAddress addr) {
        if (!nodes.contains(addr.toString())) {
            beginResetModel();
            nodes.insert(addr.toString(), "");
            endResetModel();
        }
    }

    void chatMessageReceivedSlot(QHostAddress addr, QByteArray xml) {
        QXmlQuery query;
        query.setFocus(QString::fromUtf8(xml.constData(), xml.size()));
        query.setQuery("/chat/message/string()");

        QString output;
        query.evaluateTo(&output);

        appendLog(addr.toString(), "Remote", output.simplified());
    }

    void appendLog(QString addr, QString who, QString s) {
        beginResetModel();
        nodes[addr] = nodes[addr] + "<" + who + "> "+ s + "\n";
        endResetModel();
    }

private:
    QMap<QString, QString> nodes;
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class Discovery : public QObject {
    Q_OBJECT
public:
    Discovery() {
        socket.bind(31337);
        connect(&socket, SIGNAL(readyRead()), SLOT(datagramReceived()));
        sendHelloDatagram();

        helloTimer.setInterval(30*1000);
        connect(&helloTimer, SIGNAL(timeout()), SLOT(sendHelloDatagram()));
        helloTimer.start();
    }

public slots:
    void sendHelloDatagram() {
        QByteArray helloDatagram("QLocalChat Hello");
        socket.writeDatagram(helloDatagram, QHostAddress::Broadcast, 31337);

    }

    void datagramReceived() {
        qint64 datagramSize = socket.pendingDatagramSize();
        QByteArray datagram;
        datagram.resize(datagramSize);
        QHostAddress addr;
        socket.readDatagram(datagram.data(), datagramSize, &addr);
        if (datagram.startsWith("QLocalChat Hello")) {
            emit nodeDiscovered(addr);
        }
    }
signals:
    void nodeDiscovered(QHostAddress addr);

private:
    QUdpSocket socket;
    QTimer helloTimer;

};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class HttpHandler : public QObject {
    Q_OBJECT
public:
    HttpHandler(QTcpSocket *so)
        : socket(so), contentLength(0), state(Connected)
    {
        socket->setParent(this);
        connect(socket, SIGNAL(readyRead()), SLOT(readyReadSlot()));
    }
public slots:
    void readyReadSlot() {
        while (state != ReadingData && socket->canReadLine()) {
            QByteArray line = socket->readLine().simplified();
            if (state == Connected && line == "POST /chat HTTP/1.0" || line == "POST /chat HTTP/1.1") {
                state = ReadingHeaders;
            } else if (state == Connected) {
                delete this;
                return;
            } else if (state == ReadingHeaders && line.length() > 0) {
                if (line.startsWith("Content-Length:")) {
                    contentLength = line.mid(15).toInt();
                }
                state = ReadingHeaders;
            } else if (state == ReadingHeaders && line.length() == 0) {
                state = ReadingData;
                // send reply
                socket->write("HTTP/1.0 200 OK\r\n");
                socket->write("Connection: close\r\n");
                socket->write("\r\n");
            }
        }
        if (state == ReadingData && socket->bytesAvailable()) {
            if (socket->bytesAvailable() >= contentLength) {
                QByteArray data = socket->readAll();
                emit chatMessageReceived(socket->peerAddress(), data);
                QObject::connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
                socket->disconnectFromHost();
                return;
            }
        }
    }

signals:
    void chatMessageReceived(QHostAddress, QByteArray);

private:
    enum ConnectionState {Connected, ReadingHeaders, ReadingData, Closed};
    ConnectionState state;
    QTcpSocket *socket;
    int contentLength;
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class HttpServer : public QObject {
    Q_OBJECT
public:
    HttpServer() {
        QObject::connect(&serverSocket, SIGNAL(newConnection()), SLOT(incomingConnection()));
        serverSocket.listen(QHostAddress::Any, 31337);
    }

public slots:
    void incomingConnection() {
        QTcpSocket *incomingSocket = serverSocket.nextPendingConnection();
        HttpHandler *httpHandler = new HttpHandler(incomingSocket);
        httpHandler->setParent(this);
        QObject::connect(httpHandler, SIGNAL(chatMessageReceived(QHostAddress,QByteArray)), this, SIGNAL(chatMessageReceived(QHostAddress,QByteArray)));
    }

signals:
    void chatMessageReceived(QHostAddress, QByteArray);

private:
    QTcpServer serverSocket;
};

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QmlApplicationViewer viewer;

    NodeList nodeList;
    viewer.rootContext()->setContextProperty("nodeList", &nodeList);

    Discovery localChatDiscovery;
    QObject::connect(&localChatDiscovery, SIGNAL(nodeDiscovered(QHostAddress)),
                     &nodeList, SLOT(nodeDiscoveredSlot(QHostAddress)));

    HttpServer localChatHttpServer;
    QObject::connect(&localChatHttpServer, SIGNAL(chatMessageReceived(QHostAddress, QByteArray)),
                     &nodeList, SLOT(chatMessageReceivedSlot(QHostAddress, QByteArray)));

    // QML loading
    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.setMainQmlFile(QLatin1String("qml/QLocalChat1/main.qml"));
    viewer.showExpanded();
    return app.exec();
}

#include "main.moc"