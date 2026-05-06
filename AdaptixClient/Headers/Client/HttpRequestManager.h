#ifndef ADAPTIXCLIENT_HTTPREQUESTMANAGER_H
#define ADAPTIXCLIENT_HTTPREQUESTMANAGER_H

#include <main.h>
#include <functional>

class AuthProfile;

using HttpCallback = std::function<void(bool success, const QString& message, const QJsonObject& response)>;

struct RequestContext {
    int         requestId;
    QString     url;
    QString     accessToken;
    HttpCallback callback;
    QNetworkReply* reply;
    QDateTime   startTime;
    int         retryCount;
    int         maxRetries;
    QByteArray  requestData;
    int         timeout;
};

class HttpRequestManager : public QObject
{
Q_OBJECT

    QNetworkAccessManager*     m_manager;
    QHash<int, RequestContext> m_pendingRequests;
    QHash<QNetworkReply*, int> m_replyToRequestId;
    QHash<int, QTimer*>        m_timeoutTimers;
    int                        m_requestIdCounter;
    QSslConfiguration          m_sslConfig;

    explicit HttpRequestManager(QObject* parent = nullptr);
    ~HttpRequestManager();

    HttpRequestManager(const HttpRequestManager&) = delete;
    HttpRequestManager& operator=(const HttpRequestManager&) = delete;

    void setupSslConfig();
    QNetworkRequest createRequest(const QString& url, const QString& accessToken);
    void processResponse(QNetworkReply* reply);
    void retryRequest(int requestId);
    void cleanupRequest(int requestId);

    QString buildFullUrl(const QString& baseUrl, const QString& endpoint) const;

public:
    static HttpRequestManager& instance();

    int post(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData, const HttpCallback &callback, int timeout = 8000);
    int postWithRetry(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData, const HttpCallback &callback, int maxRetries = 3, int timeout = 8000);

    void postFireAndForget(const QString& baseUrl, const QString& endpoint, const QString& accessToken, const QByteArray& jsonData);

    bool cancel(int requestId);
    void cancelAll();

    int pendingRequestsCount() const;

Q_SIGNALS:
    void requestStarted(int requestId);
    void requestFinished(int requestId, bool success, const QString& message);
    void requestProgress(int requestId, qint64 bytesSent, qint64 bytesTotal);

private Q_SLOTS:
    void onReplyFinished(QNetworkReply* reply);
    void onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
    void onRequestTimeout();
};

inline QJsonArray toJsonArray(const QStringList& list) {
    QJsonArray arr;
    for (const QString& item : list)
        arr.append(item);
    return arr;
}

#endif // ADAPTIXCLIENT_HTTPREQUESTMANAGER_H
