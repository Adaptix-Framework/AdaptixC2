#include <Client/TerminalWorker.h>

TerminalWorker::TerminalWorker(TerminalWidget* terminalWidget, const QString &token, const QUrl& wsUrl, const QString& terminalData, QObject* parent) : QObject(parent)
{
    this->terminalWidget = terminalWidget;
    this->token = token;
    this->wsUrl = wsUrl;
    this->terminalData = terminalData;
}

TerminalWorker::~TerminalWorker() = default;

void TerminalWorker::start()
{
    if (stopped)
        return;

    this->websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = websocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,                                          this, &TerminalWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived,                              this, &TerminalWorker::onWsBinaryMessageReceived, Qt::DirectConnection);
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TerminalWorker::onWsError,                 Qt::DirectConnection);

    QNetworkRequest request(wsUrl);
    request.setRawHeader("Authorization", QString("Bearer " + token).toUtf8());
    request.setRawHeader("Channel-Type",  QString("terminal").toUtf8());
    request.setRawHeader("Channel-Data",  QString(this->terminalData).toUtf8());

    websocket->open(request);
}

void TerminalWorker::stop()
{
     if (stopped.exchange(true))
         return;

     if (websocket) {
         websocket->blockSignals(true);
         websocket->close();
     }

     emit finished();
}

void TerminalWorker::onWsConnected() {}

void TerminalWorker::onWsBinaryMessageReceived(const QByteArray& msg) {
    if (stopped) return;

    if (!started) {
        started = true;

        connect(this->terminalWidget->Konsole(), &QTermWidget::sendData, this, [this](const char *data, int size) {
            if (websocket->state() == QAbstractSocket::ConnectedState) {
                websocket->sendBinaryMessage(QByteArray(data, size));
            }
        });
    }

    if (!msg.isEmpty())
        emit binaryMessageToTerminal(msg);
}

void TerminalWorker::onWsError(QAbstractSocket::SocketError error)
{
    stop();
}
