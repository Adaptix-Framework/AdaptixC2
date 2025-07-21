#ifndef TERMINALWORKER_H
#define TERMINALWORKER_H

#include <main.h>

class TerminalWidget;

class TerminalWorker : public QObject
{
Q_OBJECT

     TerminalWidget* terminalWidget = nullptr;
     QWebSocket* websocket = nullptr;
     QUrl        wsUrl;
     QString     token;
     QString     terminalData;
     bool        started = false;
     std::atomic<bool> stopped = false;

public:
     TerminalWorker(TerminalWidget* terminalWidget, const QString &token, const QUrl& wsUrl, const QString& terminalData, QObject* parent = nullptr);
     ~TerminalWorker() override;

signals:
     void binaryMessageToTerminal(const QByteArray& msg);
     void connectedToTerminal();
     void finished();
     void errorStop();

public slots:
     void start();
     void stop();

private slots:
     void onWsConnected();
     void onWsBinaryMessageReceived(const QByteArray& msg);
     void onWsError(QAbstractSocket::SocketError error);
};

#endif
