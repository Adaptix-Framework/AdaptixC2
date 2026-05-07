#include <Workers/TunnelWorker.h>

#include <QUrlQuery>

TunnelWorker::TunnelWorker(QTcpSocket* socket, const QString &otp, const QUrl& wsUrl, QObject* parent) : QObject(parent)
{
    this->otp = otp;
    this->wsUrl = wsUrl;
    this->tcpSocket = socket;
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
    sslConfig.setProtocol( QSsl::TlsV1_2OrLater );
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &TunnelWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &TunnelWorker::onWsBinaryMessageReceived, Qt::DirectConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &TunnelWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TunnelWorker::onWsError, Qt::DirectConnection);
#endif

    QUrl url(wsUrl);
    QUrlQuery query;
    query.addQueryItem("otp", otp);
    url.setQuery(query);

    QNetworkRequest request(url);
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
        websocket->blockSignals(true);
        websocket->close();
    }

    Q_EMIT finished();
}

void TunnelWorker::onTcpReadyRead()
{
    if (stopped || !tcpSocket || !websocket)
        return;

    QByteArray data = tcpSocket->readAll();
    if (!data.isEmpty()) {
        QMutexLocker locker(&wsBufferMutex);
        if (wsConnected.load()) {
            websocket->sendBinaryMessage(data);
        } else {
            wsBuffer.enqueue(data);
        }
    }
}

void TunnelWorker::onWsConnected()
{
    QMutexLocker locker(&wsBufferMutex);
    wsConnected.store(true);
    while (!wsBuffer.isEmpty()) {
        QByteArray data = wsBuffer.dequeue();
        websocket->sendBinaryMessage(data);
    }
}

void TunnelWorker::onWsBinaryMessageReceived(const QByteArray& msg) const
{
    if (stopped || !tcpSocket)
        return;

    tcpSocket->write(msg);
    tcpSocket->flush();
}

void TunnelWorker::onWsError(QAbstractSocket::SocketError error)
{
    stop();
}
