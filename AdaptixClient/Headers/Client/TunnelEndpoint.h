#ifndef TUNNELENDPOINT_H
#define TUNNELENDPOINT_H

#include <main.h>
#include <Client/AuthProfile.h>
#include <Client/TunnelWorker.h>

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

public:
    TunnelEndpoint(QObject* parent = nullptr);
    ~TunnelEndpoint() override;


    bool StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData);
    void SetTunnelId(const QString &tunnelId);

    bool Listen(const QJsonObject &obj);
    void Stop();

    void StopChannel(const QString& tunnelId);

private slots:
    void onStartLpfChannel();
    void onStartSocks4Channel();
    void onStartSocks5Channel();
    void onStartSocks5AuthChannel();
};

#endif //TUNNELENDPOINT_H
