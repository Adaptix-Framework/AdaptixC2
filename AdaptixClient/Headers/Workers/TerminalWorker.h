#ifndef TERMINALWORKER_H
#define TERMINALWORKER_H

#include <main.h>

class TerminalTab;

class TerminalWorker : public QObject
{
Q_OBJECT

     TerminalTab* terminalTab = nullptr;
     QWebSocket* websocket = nullptr;
     QUrl        wsUrl;
     QString     token;
     QString     terminalData;
     bool        started = false;
     std::atomic<bool> stopped = false;

public:
     TerminalWorker(TerminalTab* terminalTab, const QString &token, const QUrl& wsUrl, const QString& terminalData, QObject* parent = nullptr);
     ~TerminalWorker() override;

Q_SIGNALS:
     void binaryMessageToTerminal(const QByteArray& msg);
     void connectedToTerminal();
     void finished();
     void errorStop();

public Q_SLOTS:
     void start();
     void stop();

private Q_SLOTS:
     void onWsConnected();
     void onWsBinaryMessageReceived(const QByteArray& msg);
     void onWsError(QAbstractSocket::SocketError error);
};

#endif
