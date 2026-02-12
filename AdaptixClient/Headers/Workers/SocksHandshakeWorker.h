#ifndef SOCKSHANDSHAKEWORKER_H
#define SOCKSHANDSHAKEWORKER_H

#include <main.h>

class TunnelWorker;

class SocksHandshakeWorker : public QObject {
Q_OBJECT

    QTcpSocket* clientSock;
    QString     tunnelId;
    QString     tunnelType;
    bool        useAuth;
    QString     username;
    QString     password;
    QString     accessToken;
    QString     baseUrl;
    QUrl        wsUrl;

    bool processSocks4(QJsonObject& otpData, QString& channelId);
    bool processSocks5(QJsonObject& otpData, QString& channelId);
    static void rejectAndClose(QTcpSocket* sock, const QByteArray& response);

public:
    SocksHandshakeWorker(QTcpSocket* sock, const QString& tunnelId, const QString& type, bool useAuth, const QString& username, const QString& password, const QString& accessToken, const QString& baseUrl, const QUrl& wsUrl);
    ~SocksHandshakeWorker() override;

Q_SIGNALS:
    void workerReady(TunnelWorker* worker, const QString& channelId);
    void handshakeFailed();

public Q_SLOTS:
    void process();
};

#endif
