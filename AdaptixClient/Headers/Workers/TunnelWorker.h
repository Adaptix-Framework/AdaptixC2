#ifndef TUNNELWORKER_H
#define TUNNELWORKER_H

#include <main.h>

class TunnelWorker : public QObject {
Q_OBJECT

    QTcpSocket* tcpSocket = nullptr;
    QWebSocket* websocket = nullptr;
    QUrl        wsUrl;
    QString     otp;
    std::atomic<bool> stopped = false;

    QMutex wsBufferMutex;
    QQueue<QByteArray> wsBuffer;
    std::atomic<bool> wsConnected{false};

public:
    TunnelWorker(QTcpSocket* socket, const QString &otp, const QUrl& wsUrl, QObject* parent = nullptr);
    ~TunnelWorker() override;

Q_SIGNALS:
    void finished();

public Q_SLOTS:
    void start();
    void stop();

private Q_SLOTS:
    void onTcpReadyRead();
    void onWsConnected();
    void onWsBinaryMessageReceived(const QByteArray& msg) const;
    void onWsError(QAbstractSocket::SocketError error);
};

#endif
