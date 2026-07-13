#ifndef TUNNELWORKER_H
#define TUNNELWORKER_H

#include <main.h>

class TunnelWorker : public QObject {
Q_OBJECT

    QTcpSocket* tcpSocket = nullptr;
    QWebSocket* websocket = nullptr;
    QUrl        wsUrl;
    QString     otp;
    std::atomic<bool> stopped{false};
    std::atomic<bool> finishedEmitted{false};

    QMutex wsBufferMutex;
    QQueue<QByteArray> wsBuffer;
    std::atomic<bool> wsConnected{false};

    void finishWorker();

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
    void onTcpDisconnected();
    void onWsConnected();
    void onWsDisconnected();
    void onWsBinaryMessageReceived(const QByteArray& msg);
    void onWsError(QAbstractSocket::SocketError error);
};

#endif
