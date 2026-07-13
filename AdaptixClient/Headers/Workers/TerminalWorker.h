#ifndef TERMINALWORKER_H
#define TERMINALWORKER_H

#include <main.h>

class TerminalWorker : public QObject
{
Q_OBJECT

     QWebSocket* websocket = nullptr;
     QUrl        wsUrl;
     QString     otp;
     QString     lastStatus;
     bool        wsEverConnected = false;
     bool        gotBinaryData = false;
     std::atomic<bool> stopped{false};
     std::atomic<bool> finishedEmitted{false};

     void finishWorker();

public:
     TerminalWorker(const QString &otp, const QUrl& wsUrl, QObject* parent = nullptr);
     ~TerminalWorker() override;

Q_SIGNALS:
     void binaryMessageToTerminal(const QByteArray& msg);
     void connectedToTerminal();
     void terminalReady();
     void statusMessage(const QString& status);
     void finished();
     void errorStop(const QString& reason);

public Q_SLOTS:
     void start();
     void stop();
     void sendData(const QByteArray& data);

private Q_SLOTS:
     void onWsConnected();
     void onWsDisconnected();
     void onWsBinaryMessageReceived(const QByteArray& msg);
     void onWsTextMessageReceived(const QString& msg);
     void onWsError(QAbstractSocket::SocketError error);
};

#endif
