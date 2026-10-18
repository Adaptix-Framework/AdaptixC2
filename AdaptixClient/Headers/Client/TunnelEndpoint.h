#ifndef TUNNELENDPOINT_H
#define TUNNELENDPOINT_H

#include <main.h>

#include <QPointer>
#include <QSet>

class AuthProfile;
class TunnelWorker;
class SocksHandshakeWorker;

class TunnelEndpoint : public QObject {
Q_OBJECT

    qint64  tunnelId = 0;
    QString tunnelType;
    QUrl    wsUrl;
    quint16 lPort = 0;
    QString lHost;
    QString fHost;
    quint16 fPort = 0;
    bool    useAuth = false;
    QString username;
    QString password;

    QTcpServer*  tcpServer = nullptr;
    AuthProfile* profile   = nullptr;

    quint64 endpointGeneration = 0;
    bool    paused = false;
    QSet<qint64> pendingReverse;

    struct ChannelHandle {
        QPointer<QThread>      thread;
        QPointer<TunnelWorker> worker;
        qint64                 channelId = 0;
    };
    QMap<qint64, ChannelHandle> tunnelChannels;

    void startWorker(QTcpSocket* clientSock, const QJsonObject& otpData, qint64 channelId);
    void startReverseWorker(QTcpSocket* clientSock, const QJsonObject& otpData, qint64 channelId);
    void launchChannelWorker(QTcpSocket* clientSock, const QString& otp, qint64 channelId);
    void bindChannel(QThread* thread, TunnelWorker* worker, qint64 channelId);
    void startHandshakeWorker(QTcpSocket* clientSock, const QString& type);
    void nackReverseChannel(qint64 channelId);
    void abortOwnedSockets();
    void discardWorker(TunnelWorker* worker);
    void Pause();
    void Resume();

public:
    TunnelEndpoint(QObject* parent = nullptr);
    ~TunnelEndpoint() override;

    bool StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData);
    bool Restore(AuthProfile* profile, const TunnelData& data);
    void SetTunnelId(qint64 tunnelId);
    void SetActive(bool active);

    bool Listen(const QJsonObject &obj);
    void Stop();

    void StopChannel(qint64 channelId);
    void onReverseAccept(qint64 channelId);

private Q_SLOTS:
    void onStartLpfChannel();
    void onStartSocksChannel();
    void onWorkerReady(TunnelWorker* worker, qint64 channelId);
    void onHandshakeFailed();
};

#endif
