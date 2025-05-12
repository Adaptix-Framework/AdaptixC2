#include <Client/TunnelWorker.h>

TunnelWorker::TunnelWorker(QTcpSocket* socket, const QString &token, const QUrl& wsUrl, const QString& tunnelData, QObject* parent) : QObject(parent)
{
    this->token = token;
    this->wsUrl = wsUrl;
    this->tcpSocket = socket;
    this->tunnelData = tunnelData;
}

TunnelWorker::~TunnelWorker() = default;

void TunnelWorker::start()
{
    if (!tcpSocket || stopped)
        return;

    connect(tcpSocket, &QTcpSocket::readyRead, this, &TunnelWorker::onTcpReadyRead, Qt::DirectConnection);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &TunnelWorker::stop, Qt::DirectConnection);

    websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,                                          this, &TunnelWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived,                              this, &TunnelWorker::onWsBinaryMessageReceived, Qt::DirectConnection);
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TunnelWorker::onWsError,                 Qt::DirectConnection);

    QNetworkRequest request(wsUrl);
    request.setRawHeader("Authorization", QString("Bearer " + token).toUtf8());
    request.setRawHeader("Channel-Type",  QString("tunnel").toUtf8());
    request.setRawHeader("Channel-Data",  QString(this->tunnelData).toUtf8());

    websocket->open(request);
}

void TunnelWorker::stop()
{
    if (stopped.exchange(true))
        return;

    if (tcpSocket) {
        tcpSocket->disconnect(this);
        tcpSocket->close();
    }

    if (websocket) {
        websocket->disconnect(this);
        websocket->close();
    }

    emit finished();
}

void TunnelWorker::onTcpReadyRead() const
{
    if (stopped || !tcpSocket || !websocket)
        return;

    QByteArray data = tcpSocket->readAll();
    if (!data.isEmpty())
        websocket->sendBinaryMessage(data);
}

void TunnelWorker::onWsConnected() {}

void TunnelWorker::onWsBinaryMessageReceived(const QByteArray& msg) const
{
    if (stopped)
        return;
    tcpSocket->write(msg);
}

void TunnelWorker::onWsError(QAbstractSocket::SocketError error)
{
    stop();
}
