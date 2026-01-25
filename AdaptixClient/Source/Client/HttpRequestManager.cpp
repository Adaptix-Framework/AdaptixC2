#include <Client/HttpRequestManager.h>
#include <Client/AuthProfile.h>

HttpRequestManager& HttpRequestManager::instance()
{
    static HttpRequestManager instance;
    return instance;
}

HttpRequestManager::HttpRequestManager(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_requestIdCounter(0)
{
    setupSslConfig();
}

HttpRequestManager::~HttpRequestManager()
{
    cancelAll();
}

void HttpRequestManager::setupSslConfig()
{
    m_sslConfig = QSslConfiguration::defaultConfiguration();
    m_sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
}

QString HttpRequestManager::buildFullUrl(const QString& baseUrl, const QString& endpoint) const
{
    if (endpoint.startsWith("/"))
        return baseUrl + endpoint;
    return baseUrl + "/" + endpoint;
}

QNetworkRequest HttpRequestManager::createRequest(const QString& url, const QString& accessToken)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setSslConfiguration(m_sslConfig);

    if (!accessToken.isEmpty()) {
        QString bearerToken = "Bearer " + accessToken;
        request.setRawHeader("Authorization", bearerToken.toUtf8());
    }

    return request;
}

int HttpRequestManager::post(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData, const HttpCallback &callback, int timeout)
{
    return postWithRetry(baseUrl, endpoint, accessToken, jsonData, callback, 0, timeout);
}

void HttpRequestManager::postFireAndForget(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData)
{
    QString fullUrl = buildFullUrl(baseUrl, endpoint);
    QNetworkRequest request = createRequest(fullUrl, accessToken);

    QNetworkReply* reply = m_manager->post(request, jsonData);
    if (!reply)
        return;

    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
        Q_UNUSED(errors)
        if (reply)
            reply->ignoreSslErrors();
    });
}

int HttpRequestManager::postWithRetry(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData, const HttpCallback &callback, const int maxRetries, const int timeout)
{
    int requestId = ++m_requestIdCounter;

    QString fullUrl = buildFullUrl(baseUrl, endpoint);
    QNetworkRequest request = createRequest(fullUrl, accessToken);

    QNetworkReply* reply = m_manager->post(request, jsonData);

    RequestContext ctx;
    ctx.requestId = requestId;
    ctx.url = fullUrl;
    ctx.accessToken = accessToken;
    ctx.callback = callback;
    ctx.reply = reply;
    ctx.startTime = QDateTime::currentDateTime();
    ctx.retryCount = 0;
    ctx.maxRetries = maxRetries;
    ctx.requestData = jsonData;
    ctx.timeout = timeout;

    m_pendingRequests[requestId] = ctx;
    m_replyToRequestId[reply] = requestId;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply](const QList<QSslError>& errors) {
        onSslErrors(reply, errors);
    });

    if (timeout > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setProperty("requestId", requestId);
        connect(timer, &QTimer::timeout, this, &HttpRequestManager::onRequestTimeout);
        m_timeoutTimers[requestId] = timer;
        timer->start(timeout);
    }

    Q_EMIT requestStarted(requestId);

    return requestId;
}

bool HttpRequestManager::cancel(int requestId)
{
    if (!m_pendingRequests.contains(requestId))
        return false;

    RequestContext& ctx = m_pendingRequests[requestId];
    if (ctx.reply) {
        ctx.reply->abort();
    }

    cleanupRequest(requestId);
    return true;
}

void HttpRequestManager::cancelAll()
{
    QList<int> requestIds = m_pendingRequests.keys();
    for (int requestId : requestIds) {
        cancel(requestId);
    }
}

int HttpRequestManager::pendingRequestsCount() const
{
    return m_pendingRequests.size();
}

void HttpRequestManager::onReplyFinished(QNetworkReply* reply)
{
    if (!reply)
        return;

    if (!m_replyToRequestId.contains(reply)) {
        reply->deleteLater();
        return;
    }

    processResponse(reply);
}

void HttpRequestManager::processResponse(QNetworkReply* reply)
{
    int requestId = m_replyToRequestId.value(reply, -1);
    if (requestId == -1) {
        reply->deleteLater();
        return;
    }

    if (!m_pendingRequests.contains(requestId)) {
        reply->deleteLater();
        return;
    }

    RequestContext ctx = m_pendingRequests[requestId];

    if (m_timeoutTimers.contains(requestId)) {
        m_timeoutTimers[requestId]->stop();
    }

    bool success = false;
    QString message;
    QJsonObject jsonResponse;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);

        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            jsonResponse = jsonDoc.object();

            if (jsonResponse.contains("ok")) {
                success = jsonResponse["ok"].toBool();
            } else {
                success = true;
            }

            if (jsonResponse.contains("message")) {
                message = jsonResponse["message"].toString();
            }
        } else {
            message = "Invalid JSON response";
        }
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        message = "Request canceled";
    } else {
        message = reply->errorString();

        if (ctx.retryCount < ctx.maxRetries) {
            m_replyToRequestId.remove(reply);
            reply->deleteLater();
            retryRequest(requestId);
            return;
        }
    }

    if (ctx.callback) {
        ctx.callback(success, message, jsonResponse);
    }

    Q_EMIT requestFinished(requestId, success, message);

    cleanupRequest(requestId);
    reply->deleteLater();
}

void HttpRequestManager::retryRequest(int requestId)
{
    if (!m_pendingRequests.contains(requestId))
        return;

    RequestContext& ctx = m_pendingRequests[requestId];
    ctx.retryCount++;

    QNetworkRequest request = createRequest(ctx.url, ctx.accessToken);
    QNetworkReply* newReply = m_manager->post(request, ctx.requestData);

    ctx.reply = newReply;
    m_replyToRequestId[newReply] = requestId;

    connect(newReply, &QNetworkReply::finished, this, [this, newReply]() {
        onReplyFinished(newReply);
    });
    connect(newReply, &QNetworkReply::sslErrors, this, [this, newReply](const QList<QSslError>& errors) {
        onSslErrors(newReply, errors);
    });

    if (ctx.timeout > 0 && m_timeoutTimers.contains(requestId)) {
        m_timeoutTimers[requestId]->start(ctx.timeout);
    }
}

void HttpRequestManager::cleanupRequest(int requestId)
{
    if (m_pendingRequests.contains(requestId)) {
        QNetworkReply* reply = m_pendingRequests[requestId].reply;
        if (reply) {
            m_replyToRequestId.remove(reply);
        }
        m_pendingRequests.remove(requestId);
    }

    if (m_timeoutTimers.contains(requestId)) {
        QTimer* timer = m_timeoutTimers.take(requestId);
        timer->stop();
        timer->deleteLater();
    }
}

void HttpRequestManager::onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    Q_UNUSED(errors)
    if (reply) {
        reply->ignoreSslErrors();
    }
}

void HttpRequestManager::onRequestTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer)
        return;

    int requestId = timer->property("requestId").toInt();

    if (m_pendingRequests.contains(requestId)) {
        RequestContext& ctx = m_pendingRequests[requestId];

        if (ctx.retryCount < ctx.maxRetries) {
            if (ctx.reply) {
                m_replyToRequestId.remove(ctx.reply);
                ctx.reply->abort();
                ctx.reply->deleteLater();
                ctx.reply = nullptr;
            }
            retryRequest(requestId);
        } else {
            if (ctx.reply) {
                ctx.reply->abort();
            }

            if (ctx.callback) {
                ctx.callback(false, "Request timeout", QJsonObject());
            }

            Q_EMIT requestFinished(requestId, false, "Request timeout");
            cleanupRequest(requestId);
        }
    }
}
