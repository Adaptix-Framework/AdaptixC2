#ifndef BUILDWORKER_H
#define BUILDWORKER_H

#include <main.h>

class BuildWorker : public QObject
{
Q_OBJECT

    QWebSocket* websocket = nullptr;
    QUrl        wsUrl;
    QString     token;
    QString     buildData;
    QString     configData;
    std::atomic<bool> stopped = false;

public:
    BuildWorker(const QString &token, const QUrl& wsUrl, const QString& buildData, const QString& configData, QObject* parent = nullptr);
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
