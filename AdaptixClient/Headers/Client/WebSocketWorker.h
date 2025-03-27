#ifndef ADAPTIXCLIENT_WEBSOCKETWORKER_H
#define ADAPTIXCLIENT_WEBSOCKETWORKER_H

#include <main.h>
#include <Client/AuthProfile.h>

class WebSocketWorker : public QThread
{
Q_OBJECT
    AuthProfile* profile;

public:
    QWebSocket* webSocket = nullptr;

    explicit WebSocketWorker(AuthProfile* authProfile);
    ~WebSocketWorker() override;

    void run() override;
    void SetProfile(AuthProfile* authProfile);

public slots:
    void is_connected() const;
    void is_disconnected();
    void is_binaryMessageReceived( const QByteArray &data );

signals:
    void received_data( QByteArray data );
    void websocket_closed();
};

#endif //ADAPTIXCLIENT_WEBSOCKETWORKER_H