#include <Workers/WebSocketWorker.h>
#include <Client/AuthProfile.h>
#include <Utils/Logs.h>

WebSocketWorker::WebSocketWorker(AuthProfile* authProfile)
{
    profile = authProfile;
}

WebSocketWorker::~WebSocketWorker()
{
    delete webSocket;
};

void WebSocketWorker::run()
{
    webSocket = new QWebSocket;
    auto SslConf = webSocket->sslConfiguration();
    SslConf.setPeerVerifyMode( QSslSocket::VerifyNone );
    // 强制使用TLS 1.2或更高版本，避免降级到TLSv1
    SslConf.setProtocol( QSsl::TlsV1_2OrLater );
    // 禁用ALPN协议协商，强制使用HTTP/1.1进行WebSocket连接
    // 这对Cloudflare tunnel至关重要，因为HTTP/2不支持标准的WebSocket升级
    SslConf.setAllowedNextProtocols(QList<QByteArray>());
    webSocket->setSslConfiguration( SslConf );
    webSocket->ignoreSslErrors();

    connect( webSocket, &QWebSocket::connected,             this, &WebSocketWorker::is_connected );
    connect( webSocket, &QWebSocket::disconnected,          this, &WebSocketWorker::is_disconnected );
    connect( webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketWorker::is_binaryMessageReceived );

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect( webSocket, &QWebSocket::errorOccurred, this, &WebSocketWorker::is_error);
#else
        connect( webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &WebSocketWorker::is_error);
#endif

    QString urlTemplate = "wss://%1:%2%3/connect";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );

    QNetworkRequest request;
    request.setUrl(QUrl(sUrl));
    request.setRawHeader("Authorization", "Bearer " + profile->GetAccessToken().toUtf8());

    webSocket->open(request);
    
    // 启动事件循环，保持线程运行以处理WebSocket事件（包括Ping/Pong）
    exec();
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
    Q_EMIT this->connected();
}

void WebSocketWorker::is_disconnected()
{
    this->ok = false;
    this->message = "Disconnected from server";
    Q_EMIT this->websocket_closed();
}

void WebSocketWorker::is_binaryMessageReceived(const QByteArray &data)
{
    Q_EMIT this->received_data( data );
}