#ifndef ADAPTIXCLIENT_WEBSOCKETWORKER_H
#define ADAPTIXCLIENT_WEBSOCKETWORKER_H

#include <main.h>

class AuthProfile;

class WebSocketWorker : public QThread
{
Q_OBJECT
    AuthProfile* profile;

public:
    QWebSocket* webSocket = nullptr;
    QString message = "";
    bool ok = false;

    explicit WebSocketWorker(AuthProfile* authProfile);
    ~WebSocketWorker() override;

    void run() override;
    void SetProfile(AuthProfile* authProfile);

public slots:
    void is_connected();
    void is_disconnected();
    void is_binaryMessageReceived( const QByteArray &data );
    void is_error(QAbstractSocket::SocketError error);

signals:
    void connected();
    void ws_error();
    void received_data( QByteArray data );
    void websocket_closed();
};

#endif //ADAPTIXCLIENT_WEBSOCKETWORKER_H