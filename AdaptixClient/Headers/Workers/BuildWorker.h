#ifndef BUILDWORKER_H
#define BUILDWORKER_H

#include <main.h>

class BuildWorker : public QObject
{
Q_OBJECT

    QWebSocket* websocket = nullptr;
    QUrl        wsUrl;
    QString     otp;
    QString     configData;
    std::atomic<bool> stopped = false;

public:
    BuildWorker(const QString &otp, const QUrl& wsUrl, const QString& configData, QObject* parent = nullptr);
    ~BuildWorker() override;

Q_SIGNALS:
    void textMessageReceived(const QString& msg);
    void connected();
    void finished();
    void errorOccurred(const QString& error);

public Q_SLOTS:
    void start();
    void stop();

private Q_SLOTS:
    void onWsConnected();
    void onWsTextMessageReceived(const QString& msg);
    void onWsBinaryMessageReceived(const QByteArray& msg);
    void onWsError(QAbstractSocket::SocketError error);
    void onWsDisconnected();
};

#endif
