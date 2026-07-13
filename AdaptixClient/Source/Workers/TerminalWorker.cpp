#include <Workers/TerminalWorker.h>
#include <QMetaObject>
#include <QThread>
#include <QUrlQuery>

TerminalWorker::TerminalWorker(const QString &otp, const QUrl& wsUrl, QObject* parent) : QObject(parent)
{
    this->otp = otp;
    this->wsUrl = wsUrl;
}

TerminalWorker::~TerminalWorker() = default;

void TerminalWorker::finishWorker()
{
    if (finishedEmitted.exchange(true))
        return;

    stopped.store(true);

    if (websocket) {
        websocket->disconnect(this);
        if (websocket->state() != QAbstractSocket::UnconnectedState) {
            websocket->abort();
        }
        websocket->deleteLater();
        websocket = nullptr;
    }

    Q_EMIT finished();
}

void TerminalWorker::start()
{
    if (stopped.load())
        return;

    this->websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &TerminalWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::disconnected,          this, &TerminalWorker::onWsDisconnected,          Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &TerminalWorker::onWsBinaryMessageReceived, Qt::DirectConnection);
    connect(websocket, &QWebSocket::textMessageReceived,   this, &TerminalWorker::onWsTextMessageReceived,   Qt::DirectConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &TerminalWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TerminalWorker::onWsError, Qt::DirectConnection);
#endif

    QUrl url(wsUrl);
    QUrlQuery query;
    query.addQueryItem("otp", otp);
    url.setQuery(query);

    QNetworkRequest request(url);
    websocket->open(request);
}

void TerminalWorker::stop()
{
    if (stopped.exchange(true))
        return;

    if (websocket && websocket->state() != QAbstractSocket::UnconnectedState) {
        websocket->close();
        return;
    }

    finishWorker();
}

void TerminalWorker::onWsConnected()
{
    if (stopped.load())
        return;

    wsEverConnected = true;
    Q_EMIT connectedToTerminal();
}

void TerminalWorker::onWsDisconnected()
{
    if (!stopped.exchange(true)) {
        QString reason = lastStatus;
        if (reason.isEmpty())
            reason = wsEverConnected ? QStringLiteral("Disconnected") : QStringLiteral("Connection failed");
        Q_EMIT errorStop(reason);
    } else if (!lastStatus.isEmpty()) {
        Q_EMIT statusMessage(lastStatus);
    }

    finishWorker();
}

void TerminalWorker::onWsBinaryMessageReceived(const QByteArray& msg)
{
    if (stopped.load())
        return;

    if (!gotBinaryData) {
        gotBinaryData = true;
        Q_EMIT terminalReady();
    }

    if (!msg.isEmpty())
        Q_EMIT binaryMessageToTerminal(msg);
}

void TerminalWorker::onWsTextMessageReceived(const QString& msg)
{
    if (msg.isEmpty())
        return;

    lastStatus = msg;
    if (!stopped.load())
        Q_EMIT statusMessage(msg);
}

void TerminalWorker::onWsError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (lastStatus.isEmpty() && websocket)
        lastStatus = websocket->errorString();
}

void TerminalWorker::sendData(const QByteArray& data)
{
    if (stopped.load() || data.isEmpty())
        return;

    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, data]() { sendData(data); }, Qt::QueuedConnection);
        return;
    }

    if (stopped.load())
        return;
    if (websocket && websocket->state() == QAbstractSocket::ConnectedState)
        websocket->sendBinaryMessage(data);
}
