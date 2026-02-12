#include <Workers/WebSocketWorker.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

WebSocketWorker::WebSocketWorker(AuthProfile* authProfile)
{
    profile = authProfile;
}

WebSocketWorker::~WebSocketWorker()
{
    if (isRunning()) {
        QMetaObject::invokeMethod(this, "stopWorker", Qt::QueuedConnection);
        wait(5000);
        if (isRunning()) {
            terminate();
            wait();
        }
    }
}

void WebSocketWorker::run()
{
    if (pingTimer) {
        pingTimer->stop();
        delete pingTimer;
        pingTimer = nullptr;
    }
    if (webSocket) {
        webSocket->abort();
        delete webSocket;
        webSocket = nullptr;
    }

    webSocket = new QWebSocket;
    auto SslConf = webSocket->sslConfiguration();
    SslConf.setPeerVerifyMode( QSslSocket::VerifyNone );
    SslConf.setProtocol( QSsl::TlsV1_2OrLater );
    webSocket->setSslConfiguration( SslConf );
    webSocket->ignoreSslErrors();

    connect( webSocket, &QWebSocket::connected,             this, &WebSocketWorker::is_connected );
    connect( webSocket, &QWebSocket::disconnected,          this, &WebSocketWorker::is_disconnected );
    connect( webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketWorker::is_binaryMessageReceived );
    connect( webSocket, &QWebSocket::pong,                  this, &WebSocketWorker::is_pong );

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect( webSocket, &QWebSocket::errorOccurred, this, &WebSocketWorker::is_error);
#else
        connect( webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &WebSocketWorker::is_error);
#endif

    QJsonObject otpData;
    otpData["client_type"] = 1;
    otpData["console_team_mode"] = profile->GetConsoleMultiuser();
    QJsonArray subsArray;
    for (const QString &sub : profile->GetSubscriptions())
        subsArray.append(sub);
    otpData["subscriptions"] = subsArray;

    QString otp;
    bool otpResult = HttpReqGetOTP("connect", otpData, profile->GetURL(), profile->GetAccessToken(), &otp);
    if (!otpResult) {
        this->ok = false;
        this->message = "Failed to generate OTP for connect";
        Q_EMIT ws_error();
        return;
    }

    QString urlTemplate = "wss://%1:%2%3/connect";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );

    QUrl url(sUrl);
    QUrlQuery query;
    query.addQueryItem("otp", otp);
    url.setQuery(query);

    QNetworkRequest request;
    request.setUrl(url);

    webSocket->open(request);

    exec();
}

void WebSocketWorker::startPingTimer()
{
    if (!pingTimer) {
        pingTimer = new QTimer();
        connect(pingTimer, &QTimer::timeout, this, &WebSocketWorker::sendPing);
    }
    pingTimer->start(PING_INTERVAL_MS);
}

void WebSocketWorker::stopPingTimer()
{
    if (pingTimer) {
        pingTimer->stop();
    }
}

void WebSocketWorker::sendPing()
{
    if (webSocket && webSocket->state() == QAbstractSocket::ConnectedState) {
        webSocket->ping();
    }
}

void WebSocketWorker::is_pong(quint64 elapsedTime, const QByteArray &payload)
{
    Q_UNUSED(elapsedTime)
    Q_UNUSED(payload)
}

void WebSocketWorker::is_error(QAbstractSocket::SocketError error)
{
    QString errMsg = webSocket->errorString();

    this->ok = false;
    this->message = "Login failure";
    if (errMsg.contains("511") || errMsg.contains("Network Authentication Required"))
        this->message = "User already connected";

    Q_EMIT ws_error();
}

void WebSocketWorker::SetProfile(AuthProfile* authProfile)
{
    profile = authProfile;
}

void WebSocketWorker::is_connected()
{
    this->ok = true;
    this->message = "";
    startPingTimer();
    Q_EMIT this->connected();
}

void WebSocketWorker::is_disconnected()
{
    stopPingTimer();
    this->ok = false;
    this->message = "Disconnected from server";
    Q_EMIT this->websocket_closed();
}

void WebSocketWorker::is_binaryMessageReceived(const QByteArray &data)
{
    QMutexLocker locker(&bufferMutex);
    if (handlerReady) {
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            Q_EMIT this->received_json(jsonDoc.object());
        } else {
            Q_EMIT this->received_data(data);
        }
    } else {
        dataBuffer.append(data);
    }
}

void WebSocketWorker::setHandlerReady()
{
    QMutexLocker locker(&bufferMutex);
    handlerReady = true;
    for (const QByteArray &data : dataBuffer) {
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            Q_EMIT this->received_json(jsonDoc.object());
        } else {
            Q_EMIT this->received_data(data);
        }
    }
    dataBuffer.clear();
}

void WebSocketWorker::stopWorker()
{
    if (pingTimer) {
        pingTimer->stop();
        disconnect(pingTimer, nullptr, nullptr, nullptr);
        delete pingTimer;
        pingTimer = nullptr;
    }
    if (webSocket) {
        disconnect(webSocket, nullptr, nullptr, nullptr);
        webSocket->abort();
        delete webSocket;
        webSocket = nullptr;
    }
    quit();
}