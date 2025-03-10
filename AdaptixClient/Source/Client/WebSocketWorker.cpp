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

    QObject::connect( webSocket, &QWebSocket::connected,             this, &WebSocketWorker::is_connected );
    QObject::connect( webSocket, &QWebSocket::disconnected,          this, &WebSocketWorker::is_disconnected );
    QObject::connect( webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketWorker::is_binaryMessageReceived );

    QString urlTemplate = "wss://%1:%2%3/connect";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );
    webSocket->open( sUrl );
}

void WebSocketWorker::SetProfile(AuthProfile* authProfile)
{
    profile = authProfile;
}


void WebSocketWorker::is_connected() const
{
    QJsonObject dataJson;
    dataJson["access_token"] = profile->GetAccessToken();
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    webSocket->sendBinaryMessage( jsonData );
}

void WebSocketWorker::is_disconnected()
{
    emit this->websocket_closed();
}

void WebSocketWorker::is_binaryMessageReceived(const QByteArray &data)
{
    emit this->received_data( data );
}