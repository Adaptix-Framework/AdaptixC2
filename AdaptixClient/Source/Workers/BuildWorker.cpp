#include <Workers/BuildWorker.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrlQuery>
#include <QWebSocket>

BuildWorker::BuildWorker(const QString& otpToken, const QUrl& channelUrl, const QString& cfg, const QUrl& generateUrl, const QString& token, const QByteArray& jsonBody, QObject* parent) : QObject(parent)
{
    otp = otpToken;
    wsUrl = channelUrl;
    configData = cfg;
    httpUrl = generateUrl;
    accessToken = token;
    requestBody = jsonBody;
}

BuildWorker::~BuildWorker()
{
    stopped = true;
    if (websocket) {
        websocket->disconnect(this);
        websocket->abort();
        websocket = nullptr;
    }
    if (reply) {
        reply->disconnect(this);
        if (reply->isRunning())
            reply->abort();
        reply = nullptr;
    }
}

void BuildWorker::finishLater()
{
    QMetaObject::invokeMethod(this, [this]() { Q_EMIT finished(); }, Qt::QueuedConnection);
}

void BuildWorker::start()
{
    if (stopped)
        return;

    Q_EMIT connected();
    if (!otp.isEmpty() && wsUrl.isValid())
        startWs();
    else
        startHttp();
}

void BuildWorker::startWs()
{
    websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &BuildWorker::onWsConnected,           Qt::DirectConnection);
    connect(websocket, &QWebSocket::textMessageReceived,   this, &BuildWorker::onWsTextMessageReceived, Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &BuildWorker::onWsBinaryMessageReceived, Qt::DirectConnection);
    connect(websocket, &QWebSocket::disconnected,          this, &BuildWorker::onWsDisconnected,        Qt::DirectConnection);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &BuildWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &BuildWorker::onWsError, Qt::DirectConnection);
#endif

    QUrl url(wsUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("otp"), otp);
    url.setQuery(query);

    QNetworkRequest request(url);
    websocket->open(request);
}

void BuildWorker::startHttp()
{
    if (stopped || httpUsed.exchange(true))
        return;

    Q_EMIT textMessageReceived(QStringLiteral("{\"status\":1,\"message\":\"Starting payload build...\"}"));

    nam = new QNetworkAccessManager(this);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    QNetworkRequest request{httpUrl};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setSslConfiguration(sslConfig);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    if (!accessToken.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());

    reply = nam->post(request, requestBody);
    connect(reply, &QNetworkReply::finished, this, &BuildWorker::onHttpFinished, Qt::QueuedConnection);

    auto* timeout = new QTimer(this);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this]() {
        if (!stopped && reply && reply->isRunning())
            reply->abort();
    });
    timeout->start(180000);
}

void BuildWorker::stop()
{
    if (stopped.exchange(true))
        return;

    if (websocket) {
        websocket->disconnect(this);
        websocket->abort();
    }
    if (reply) {
        reply->disconnect(this);
        if (reply->isRunning())
            reply->abort();
    }
    finishLater();
}

void BuildWorker::onWsConnected()
{
    if (!configData.isEmpty() && websocket && websocket->state() == QAbstractSocket::ConnectedState)
        websocket->sendTextMessage(configData);
}

void BuildWorker::onWsTextMessageReceived(const QString& msg)
{
    if (stopped)
        return;
    if (msg.isEmpty())
        return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("status")).toInt() == 4)
            gotFile = true;
    }

    Q_EMIT textMessageReceived(msg);
}

void BuildWorker::onWsBinaryMessageReceived(const QByteArray& msg)
{
    if (stopped)
        return;
    if (!msg.isEmpty())
        onWsTextMessageReceived(QString::fromUtf8(msg));
}

void BuildWorker::onWsError(QAbstractSocket::SocketError)
{
    if (stopped || gotFile)
        return;
    const QString err = websocket ? websocket->errorString() : QStringLiteral("WebSocket error");
    Q_EMIT textMessageReceived(QStringLiteral("{\"status\":1,\"message\":\"Live log channel unavailable (%1)\"}").arg(err));
}

void BuildWorker::onWsDisconnected()
{
    if (stopped)
        return;
    if (gotFile) {
        if (!stopped.exchange(true))
            finishLater();
        return;
    }
    if (!httpUrl.isEmpty())
        startHttp();
    else if (!stopped.exchange(true))
        finishLater();
}

void BuildWorker::onHttpFinished()
{
    if (stopped.exchange(true))
        return;

    QNetworkReply* r = reply;
    reply = nullptr;

    QString err;
    QByteArray raw;
    if (!r)
        err = QStringLiteral("no HTTP reply");
    else {
        if (r->error() != QNetworkReply::NoError && r->error() != QNetworkReply::OperationCanceledError)
            err = r->errorString();
        raw = r->readAll();
        r->deleteLater();
    }

    if (r && r->error() == QNetworkReply::OperationCanceledError) {
        finishLater();
        return;
    }

    if (!err.isEmpty() && raw.isEmpty()) {
        Q_EMIT errorOccurred(err);
        finishLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        Q_EMIT errorOccurred(err.isEmpty() ? QStringLiteral("invalid generate response") : err);
        finishLater();
        return;
    }

    const QJsonObject obj = doc.object();
    if (!obj.value(QStringLiteral("ok")).toBool()) {
        Q_EMIT errorOccurred(obj.value(QStringLiteral("message")).toString());
        finishLater();
        return;
    }

    const QString message = obj.value(QStringLiteral("message")).toString();
    const int colon = message.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        Q_EMIT errorOccurred(QStringLiteral("malformed generate payload"));
        finishLater();
        return;
    }

    const QString filename = QString::fromUtf8(QByteArray::fromBase64(message.left(colon).toUtf8()));
    const QByteArray content = QByteArray::fromBase64(message.mid(colon + 1).toUtf8());
    if (filename.isEmpty() || content.isEmpty()) {
        Q_EMIT errorOccurred(QStringLiteral("empty generate payload"));
        finishLater();
        return;
    }

    gotFile = true;
    Q_EMIT fileReady(filename, content);
    finishLater();
}
