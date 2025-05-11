#ifndef TUNNELENDPOINT_H
#define TUNNELENDPOINT_H

#include <main.h>
#include <Client/AuthProfile.h>
#include <Client/TunnelWorker.h>

class TunnelEndpoint : public QObject {
Q_OBJECT

    QString      tunnelId;
    QString      tunnelType;
    QTcpServer*  tcpServer = nullptr;
    quint16      lPort = 0;
    QString      lHost;
    QUrl         wsUrl;
    AuthProfile* profile = nullptr;

    struct ChannelHandle {
        QThread*      thread;
        TunnelWorker* worker;
    };
    QMap<QString, ChannelHandle>  tunnelChannels;

public:
    TunnelEndpoint(QObject* parent = nullptr);
    ~TunnelEndpoint() override;


    bool StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData);
    void SetTunnelId(const QString &tunnelId);

    bool StartLocalPortFwd(const QByteArray &jsonData);
    void Stop();

    void StopChannel(const QString& tunnelId);

private slots:
    void onStartLpfChannel();
    void onStartSocks4Channel();
};

#endif //TUNNELENDPOINT_H
