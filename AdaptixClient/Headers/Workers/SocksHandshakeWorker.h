#ifndef SOCKSHANDSHAKEWORKER_H
#define SOCKSHANDSHAKEWORKER_H

#include <main.h>
#include <Client/AuthProfile.h>

class TunnelWorker;

class SocksHandshakeWorker : public QObject {
Q_OBJECT

    QTcpSocket* clientSock;
    qint64      tunnelId;
    QString     tunnelType;
    bool        useAuth;
    QString     username;
    QString     password;
    AuthProfile profile;
    QUrl        wsUrl;

    bool processSocks4(QJsonObject& otpData, qint64& channelId);
    bool processSocks5(QJsonObject& otpData, qint64& channelId);
    static void rejectAndClose(QTcpSocket* sock, const QByteArray& response);

public:
    SocksHandshakeWorker(QTcpSocket* sock, qint64 tunnelId, const QString& type, bool useAuth, const QString& username, const QString& password, const AuthProfile& profile, const QUrl& wsUrl);
    ~SocksHandshakeWorker() override;

Q_SIGNALS:
    void workerReady(TunnelWorker* worker, qint64 channelId);
    void handshakeFailed();

public Q_SLOTS:
    void process();
};

#endif
