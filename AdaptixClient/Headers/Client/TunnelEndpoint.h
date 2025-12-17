#ifndef TUNNELENDPOINT_H
#define TUNNELENDPOINT_H

#include <main.h>

class AuthProfile;
class TunnelWorker;
class SocksHandshakeWorker;

class TunnelEndpoint : public QObject {
Q_OBJECT

    QString tunnelId;
    QString tunnelType;
    QUrl    wsUrl;
    quint16 lPort = 0;
    QString lHost;
    bool    useAuth = false;
    QString username;
    QString password;

    QTcpServer*  tcpServer = nullptr;
    AuthProfile* profile   = nullptr;

    struct ChannelHandle {
        QThread*      thread;
        TunnelWorker* worker;
        QString       channelId;
    };
    QMap<QString, ChannelHandle>  tunnelChannels;

    void startWorker(QTcpSocket* clientSock, const QString& tunnelData);
    void startHandshakeWorker(QTcpSocket* clientSock, const QString& type);

public:
    TunnelEndpoint(QObject* parent = nullptr);
    ~TunnelEndpoint() override;


    bool StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData);
    void SetTunnelId(const QString &tunnelId);

    bool Listen(const QJsonObject &obj);
    void Stop();

    void StopChannel(const QString& tunnelId);

private Q_SLOTS:
    void onStartLpfChannel();
    void onStartSocksChannel();
    void onWorkerReady(TunnelWorker* worker, const QString& channelId);
    void onHandshakeFailed();
};

#endif
