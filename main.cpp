// This code is released as public domain.
// -- Markus Goetz
#include <QGuiApplication>
#include <QUdpSocket>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QAbstractTableModel>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>

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
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[AddrRole] = "addr";
        roles[ChatLogRole] = "log";
        return roles;
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const {
        Q_UNUSED(parent);
        return nodes.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const {
        Q_UNUSED(parent);
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

    void chatMessageReceivedSlot(QHostAddress addr, QByteArray json) {
        QJsonDocument jsonDocument = QJsonDocument::fromJson(json);
        QString msg = jsonDocument.object().value("chat").toObject().value("message").toString();
        appendLog(addr.toString(), "Remote", msg.simplified());
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
        connect(&socket, &QUdpSocket::readyRead, this, &Discovery::datagramReceived);
        sendHelloDatagram();

        helloTimer.setInterval(30*1000);
        connect(&helloTimer, &QTimer::timeout, this, &Discovery::sendHelloDatagram);
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
        : state(Connected), socket(so), contentLength(0)
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, &HttpHandler::readyReadSlot);
    }
public slots:
    void readyReadSlot() {
        while (state != ReadingData && socket->canReadLine()) {
            QByteArray line = socket->readLine().simplified();
            if (state == Connected && (line == "POST /chat HTTP/1.0" || line == "POST /chat HTTP/1.1")) {
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
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
                socket->disconnectFromHost();
                return;
            }
        }
    }

signals:
    void chatMessageReceived(QHostAddress, QByteArray);

private:
    enum ConnectionState {Connected, ReadingHeaders,
                          ReadingData, Closed};
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
        QObject::connect(&serverSocket,&QTcpServer::newConnection,
                         this, &HttpServer::incomingConnection);
        serverSocket.listen(QHostAddress::Any, 31337);
    }

public slots:
    void incomingConnection() {
        QTcpSocket *incomingSocket = serverSocket.nextPendingConnection();
        HttpHandler *httpHandler = new HttpHandler(incomingSocket);
        httpHandler->setParent(this);
        // Just forward the received message as signal
        QObject::connect(httpHandler, &HttpHandler::chatMessageReceived,
                         this, &HttpServer::chatMessageReceived);
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
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    NodeList nodeList;
    engine.rootContext()->setContextProperty("nodeList", &nodeList);

    Discovery localChatDiscovery;
    QObject::connect(&localChatDiscovery, &Discovery::nodeDiscovered,
                     &nodeList, &NodeList::nodeDiscoveredSlot);

    HttpServer localChatHttpServer;
    QObject::connect(&localChatHttpServer, &HttpServer::chatMessageReceived,
                     &nodeList, &NodeList::chatMessageReceivedSlot);

    // QML loading
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    return app.exec();
}

#include "main.moc"
