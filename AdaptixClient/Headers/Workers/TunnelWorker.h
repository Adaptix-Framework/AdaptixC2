#ifndef TUNNELWORKER_H
#define TUNNELWORKER_H

#include <main.h>

class TunnelWorker : public QObject {
Q_OBJECT

    QTcpSocket* tcpSocket = nullptr;
    QWebSocket* websocket = nullptr;
    QUrl        wsUrl;
    QString     token;
    QString     tunnelData;
    std::atomic<bool> stopped = false;

    QMutex wsBufferMutex;
    QQueue<QByteArray> wsBuffer;
    bool wsConnected = false;

public:
    TunnelWorker(QTcpSocket* socket, const QString &token, const QUrl& wsUrl, const QString& tunnelData, QObject* parent = nullptr);
    ~TunnelWorker() override;

signals:
    void finished();

public slots:
    void start();
    void stop();

private slots:
    void onTcpReadyRead();
    void onWsConnected();
    void onWsBinaryMessageReceived(const QByteArray& msg) const;
    void onWsError(QAbstractSocket::SocketError error);
};

#endif //TUNNELWORKER_H
