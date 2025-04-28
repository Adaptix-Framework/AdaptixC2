#include <Client/WebSocketWorker.h>

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
    webSocket->setSslConfiguration( SslConf );
    webSocket->ignoreSslErrors();

    connect( webSocket, &QWebSocket::connected,             this, &WebSocketWorker::is_connected );
    connect( webSocket, &QWebSocket::disconnected,          this, &WebSocketWorker::is_disconnected );
    connect( webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketWorker::is_binaryMessageReceived );
    connect( webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &WebSocketWorker::is_error);

    QString urlTemplate = "wss://%1:%2%3/connect";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );

    QNetworkRequest request;
    request.setUrl(QUrl(sUrl));
    request.setRawHeader("Authorization", "Bearer " + profile->GetAccessToken().toUtf8());

    webSocket->open(request);
}

void WebSocketWorker::is_error(QAbstractSocket::SocketError error)
{
    QString errMsg = webSocket->errorString();

    this->ok = false;
    this->message = "Login failure";
    if (errMsg.contains("511") || errMsg.contains("Network Authentication Required"))
        this->message = "User already connected";

    emit ws_error();
}

void WebSocketWorker::SetProfile(AuthProfile* authProfile)
{
    profile = authProfile;
}

void WebSocketWorker::is_connected()
{
    this->ok = true;
    this->message = "";
    emit this->connected();
}

void WebSocketWorker::is_disconnected()
{
    this->ok = false;
    this->message = "Disconnected from server";
    emit this->websocket_closed();
}

void WebSocketWorker::is_binaryMessageReceived(const QByteArray &data)
{
    emit this->received_data( data );
}