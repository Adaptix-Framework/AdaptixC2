#include <Workers/TerminalWorker.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <QMetaObject>

TerminalWorker::TerminalWorker(TerminalTab* terminalTab, const QString &token, const QUrl& wsUrl, const QString& terminalData, QObject* parent) : QObject(parent)
{
    this->terminalTab = terminalTab;
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
    sslConfig.setProtocol( QSsl::TlsV1_2OrLater );
    websocket->setSslConfiguration(sslConfig);
    websocket->ignoreSslErrors();

    connect(websocket, &QWebSocket::connected,             this, &TerminalWorker::onWsConnected,             Qt::DirectConnection);
    connect(websocket, &QWebSocket::binaryMessageReceived, this, &TerminalWorker::onWsBinaryMessageReceived, Qt::DirectConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(websocket, &QWebSocket::errorOccurred, this, &TerminalWorker::onWsError, Qt::DirectConnection);
#else
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TerminalWorker::onWsError, Qt::DirectConnection);
#endif

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
        if (websocket->state() != QAbstractSocket::UnconnectedState) {
            connect(websocket, &QWebSocket::disconnected, this, [this]() {
                websocket->deleteLater();
                Q_EMIT finished();
                this->deleteLater();
            });

            websocket->close();
            return;
        } else {
            websocket->deleteLater();
        }
    }

    Q_EMIT finished();
    this->deleteLater();
}

void TerminalWorker::onWsConnected() {}

void TerminalWorker::onWsBinaryMessageReceived(const QByteArray& msg) {
    if (stopped) return;

    if (!started) {
        started = true;
        Q_EMIT connectedToTerminal();
    }

    if (!msg.isEmpty())
        Q_EMIT binaryMessageToTerminal(msg);
}

void TerminalWorker::onWsError(QAbstractSocket::SocketError error)
{
    Q_EMIT errorStop();
}

void TerminalWorker::sendData(const QByteArray& data)
{
    if (stopped)
        return;

    QMetaObject::invokeMethod(this, [this, data]() {
        if (stopped)
            return;
        if (websocket && websocket->state() == QAbstractSocket::ConnectedState) {
            websocket->sendBinaryMessage(data);
        }
    }, Qt::QueuedConnection);
}
