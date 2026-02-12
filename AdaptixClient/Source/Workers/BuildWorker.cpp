#include <Workers/BuildWorker.h>

#include <QUrlQuery>

BuildWorker::BuildWorker(const QString &otp, const QUrl& wsUrl, const QString& configData, QObject* parent) : QObject(parent)
{
    this->otp = otp;
    this->wsUrl = wsUrl;
    this->configData = configData;
}

BuildWorker::~BuildWorker() = default;

void BuildWorker::start()
{
    if (stopped)
        return;

    this->websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol( QSsl::TlsV1_2OrLater );
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &BuildWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::textMessageReceived,   this, &BuildWorker::onWsTextMessageReceived,   Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &BuildWorker::onWsBinaryMessageReceived, Qt::DirectConnection);
    connect(websocket, &QWebSocket::disconnected,          this, &BuildWorker::onWsDisconnected,          Qt::DirectConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &BuildWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &BuildWorker::onWsError, Qt::DirectConnection);
#endif

    QUrl url(wsUrl);
    QUrlQuery query;
    query.addQueryItem("otp", otp);
    url.setQuery(query);

    QNetworkRequest request(url);
    websocket->open(request);
}

void BuildWorker::stop()
{
    if (stopped.exchange(true))
        return;

    if (websocket) {
        if (websocket->state() != QAbstractSocket::UnconnectedState) {
            connect(websocket, &QWebSocket::disconnected, this, [this]() {
                websocket->deleteLater();
                Q_EMIT finished();
                this->deleteLater();
            });

            websocket->close();
            return;
        } else {
            websocket->deleteLater();
        }
    }

    Q_EMIT finished();
    this->deleteLater();
}

void BuildWorker::onWsConnected()
{
    if (!configData.isEmpty() && websocket && websocket->state() == QAbstractSocket::ConnectedState) {
        websocket->sendTextMessage(configData);
    }
    Q_EMIT connected();
}

void BuildWorker::onWsTextMessageReceived(const QString& msg)
{
    if (stopped)
        return;

    if (!msg.isEmpty())
        Q_EMIT textMessageReceived(msg);
}

void BuildWorker::onWsBinaryMessageReceived(const QByteArray& msg)
{
    if (stopped)
        return;

    if (!msg.isEmpty())
        Q_EMIT textMessageReceived(QString::fromUtf8(msg));
}

void BuildWorker::onWsError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errorMsg = websocket ? websocket->errorString() : "Unknown error";
    Q_EMIT errorOccurred(errorMsg);
}

void BuildWorker::onWsDisconnected()
{
    if (!stopped.exchange(true)) {
        Q_EMIT finished();
    }
}
