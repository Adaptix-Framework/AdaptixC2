#ifndef ADAPTIXCLIENT_WEBSOCKETWORKER_H
#define ADAPTIXCLIENT_WEBSOCKETWORKER_H

#include <main.h>
#include <Client/AuthProfile.h>

class WebSocketWorker : public QThread
{
Q_OBJECT
    QWebSocket* webSocket = nullptr;
    AuthProfile profile;

public:
    explicit WebSocketWorker(AuthProfile authProfile);
    ~WebSocketWorker();

    void run();

public slots:
    void is_connected();
    void is_disconnected();
    void is_binaryMessageReceived( const QByteArray &data );

signals:
    void received_data( QByteArray data );
    void websocket_closed();
};

#endif //ADAPTIXCLIENT_WEBSOCKETWORKER_H