#include <Workers/TunnelWorker.h>

#include <QThread>
#include <QUrlQuery>

TunnelWorker::TunnelWorker(QTcpSocket* socket, const QString &otp, const QUrl& wsUrl, QObject* parent) : QObject(parent)
{
    this->otp = otp;
    this->wsUrl = wsUrl;
    this->tcpSocket = socket;
}

TunnelWorker::~TunnelWorker() = default;

void TunnelWorker::finishWorker()
{
    if (finishedEmitted.exchange(true))
        return;

    stopped.store(true);

    if (tcpSocket) {
        QObject::disconnect(tcpSocket, nullptr, this, nullptr);
        if (tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            QSignalBlocker blocker(tcpSocket);
            tcpSocket->close();
        }
    }

    if (websocket) {
        QObject::disconnect(websocket, nullptr, this, nullptr);
        websocket->blockSignals(true);
        if (websocket->state() != QAbstractSocket::UnconnectedState)
            websocket->close();
        websocket->deleteLater();
        websocket = nullptr;
    }

    Q_EMIT finished();
}

void TunnelWorker::start()
{
    if (!tcpSocket || stopped.load())
        return;

    if (tcpSocket->thread() != QThread::currentThread())
        tcpSocket->moveToThread(QThread::currentThread());

    connect(tcpSocket, &QTcpSocket::readyRead,    this, &TunnelWorker::onTcpReadyRead,    Qt::DirectConnection);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &TunnelWorker::onTcpDisconnected, Qt::DirectConnection);

    websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &TunnelWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::disconnected,          this, &TunnelWorker::onWsDisconnected,          Qt::DirectConnection);
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

    bool waiting = false;

    if (tcpSocket && tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        tcpSocket->disconnectFromHost();
        waiting = true;
    }

    if (websocket && websocket->state() != QAbstractSocket::UnconnectedState) {
        websocket->close();
        waiting = true;
    }

    if (!waiting)
        finishWorker();
}

void TunnelWorker::onTcpReadyRead()
{
    if (stopped.load() || !tcpSocket || !websocket)
        return;

    const QByteArray data = tcpSocket->readAll();
    if (data.isEmpty())
        return;

    QMutexLocker locker(&wsBufferMutex);
    if (wsConnected.load() && websocket->state() == QAbstractSocket::ConnectedState) {
        websocket->sendBinaryMessage(data);
    } else {
        constexpr int kMaxBufferedChunks = 256;
        if (wsBuffer.size() >= kMaxBufferedChunks)
            wsBuffer.dequeue();
        wsBuffer.enqueue(data);
    }
}

void TunnelWorker::onTcpDisconnected()
{
    stopped.store(true);
    finishWorker();
}

void TunnelWorker::onWsConnected()
{
    if (stopped.load())
        return;

    QMutexLocker locker(&wsBufferMutex);
    wsConnected.store(true);
    while (!wsBuffer.isEmpty()) {
        if (!websocket || websocket->state() != QAbstractSocket::ConnectedState)
            break;
        websocket->sendBinaryMessage(wsBuffer.dequeue());
    }
}

void TunnelWorker::onWsDisconnected()
{
    stopped.store(true);
    finishWorker();
}

void TunnelWorker::onWsBinaryMessageReceived(const QByteArray& msg)
{
    if (stopped.load() || !tcpSocket || msg.isEmpty())
        return;

    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->write(msg);
        tcpSocket->flush();
    }
}

void TunnelWorker::onWsError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (!websocket || websocket->state() == QAbstractSocket::UnconnectedState) {
        stopped.store(true);
        finishWorker();
    }
}
