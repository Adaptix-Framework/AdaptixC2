#ifndef ADAPTIXCLIENT_WEBSOCKETWORKER_H
#define ADAPTIXCLIENT_WEBSOCKETWORKER_H

#include <main.h>

#define PING_INTERVAL_MS 15000

class AuthProfile;


class WebSocketWorker : public QThread
{
Q_OBJECT
    AuthProfile* profile;
    QTimer* pingTimer = nullptr;
    QList<QByteArray> dataBuffer;
    bool handlerReady = false;
    QMutex bufferMutex;

public:
    QWebSocket* webSocket = nullptr;
    QString message = "";
    bool ok = false;

    explicit WebSocketWorker(AuthProfile* authProfile);
    ~WebSocketWorker() override;

    void run() override;
    void SetProfile(AuthProfile* authProfile);
    void startPingTimer();
    void stopPingTimer();
    void setHandlerReady();

public Q_SLOTS:
    void is_connected();
    void is_disconnected();
    void is_binaryMessageReceived( const QByteArray &data );
    void is_error(QAbstractSocket::SocketError error);
    void is_pong(quint64 elapsedTime, const QByteArray &payload);
    void sendPing();
    void stopWorker();

Q_SIGNALS:
    void connected();
    void ws_error();
    void received_data( QByteArray data );
    void websocket_closed();
};

#endif