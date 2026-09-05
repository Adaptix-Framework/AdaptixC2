#ifndef BUILDWORKER_H
#define BUILDWORKER_H

#include <main.h>

class QNetworkAccessManager;
class QNetworkReply;
class QWebSocket;

class BuildWorker : public QObject
{
Q_OBJECT

    QWebSocket*            websocket = nullptr;
    QNetworkAccessManager* nam   = nullptr;
    QNetworkReply*         reply = nullptr;

    QString   otp;
    QUrl      wsUrl;
    QString   configData;
    QUrl      httpUrl;
    QString   accessToken;
    QByteArray requestBody;

    std::atomic<bool> stopped  = false;
    std::atomic<bool> gotFile  = false;
    std::atomic<bool> httpUsed = false;

    void finishLater();
    void startWs();
    void startHttp();

public:
    BuildWorker(const QString& otp, const QUrl& wsUrl, const QString& configData, const QUrl& generateUrl, const QString& token, const QByteArray& jsonBody, QObject* parent = nullptr);
    ~BuildWorker() override;

Q_SIGNALS:
    void textMessageReceived(const QString& msg);
    void fileReady(const QString& filename, const QByteArray& content);
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
    void onHttpFinished();
};

#endif
