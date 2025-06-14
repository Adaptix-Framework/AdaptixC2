#include <Workers/TunnelWorker.h>

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

    connect(websocket, &QWebSocket::connected,             this, &TunnelWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &TunnelWorker::onWsBinaryMessageReceived, Qt::DirectConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &TunnelWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TunnelWorker::onWsError, Qt::DirectConnection);
#endif

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
        QObject::disconnect(tcpSocket, &QTcpSocket::readyRead,    this, &TunnelWorker::onTcpReadyRead);
        QObject::disconnect(tcpSocket, &QTcpSocket::disconnected, this, &TunnelWorker::stop);

        QSignalBlocker blocker(tcpSocket);
        tcpSocket->close();
    }

    if (websocket) {
        // websocket->disconnect(this);
        websocket->blockSignals(true);
        websocket->close();
    }

    emit finished();
}

void TunnelWorker::onTcpReadyRead()
{
    if (stopped || !tcpSocket || !websocket)
        return;

    QByteArray data = tcpSocket->readAll();
    if (!data.isEmpty()) {
        if (wsConnected) {
            websocket->sendBinaryMessage(data);
        } else {
            wsBuffer.enqueue(data);
        }
    }
}

void TunnelWorker::onWsConnected()
{
    {
        QMutexLocker locker(&wsBufferMutex);
        while (!wsBuffer.isEmpty()) {
            QByteArray data = wsBuffer.dequeue();
            websocket->sendBinaryMessage(data);
        }
    }

    wsConnected = true;
}

void TunnelWorker::onWsBinaryMessageReceived(const QByteArray& msg) const
{
    if (stopped)
        return;

    tcpSocket->write(msg);
    tcpSocket->flush();
}

void TunnelWorker::onWsError(QAbstractSocket::SocketError error)
{
    stop();
}
