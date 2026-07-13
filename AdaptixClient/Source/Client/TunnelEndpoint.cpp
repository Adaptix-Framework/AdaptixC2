#include <Workers/TunnelWorker.h>
#include <Workers/SocksHandshakeWorker.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <QRandomGenerator>
#include <QPointer>

TunnelEndpoint::TunnelEndpoint(QObject* parent) : QObject(parent), tcpServer(new QTcpServer(this)) {}

TunnelEndpoint::~TunnelEndpoint()
{
    Stop();
}

bool TunnelEndpoint::StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData)
{
    this->profile = profile;

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg(profile->GetHost()).arg(profile->GetPort()).arg(profile->GetEndpoint());
    this->wsUrl = QUrl(sUrl);

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject obj = doc.object();

    this->tunnelType = type;

    if (type == "socks5") {
        this->useAuth = obj["use_auth"].toBool();
        this->username = obj["username"].toString();
        this->password = obj["password"].toString();
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocksChannel);
        return Listen(obj);
    }
    if (type == "socks4") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocksChannel);
        return Listen(obj);
    }
    if (type == "lportfwd") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartLpfChannel);
        return Listen(obj);
    }
    if (type == "rportfwd") {
        return false;
    }
    return false;
}

bool TunnelEndpoint::Listen(const QJsonObject &obj)
{
    lHost = obj["l_host"].toString();
    lPort = static_cast<quint16>(obj["l_port"].toInt());

    if (!tcpServer->listen(QHostAddress(lHost), lPort)) {
        MessageError(tcpServer->errorString());
        return false;
    }
    return true;
}

void TunnelEndpoint::SetTunnelId(qint64 tunnelId)
{
    this->tunnelId = tunnelId;
}

void TunnelEndpoint::StopChannel(qint64 channelId)
{
    auto it = tunnelChannels.find(channelId);
    if (it == tunnelChannels.end())
        return;

    if (it->worker)
        QMetaObject::invokeMethod(it->worker, "stop", Qt::QueuedConnection);
}

void TunnelEndpoint::Stop()
{
    ++endpointGeneration;

    if (tcpServer && tcpServer->isListening())
        tcpServer->close();

    const QList<QTcpSocket*> pendingSocks = findChildren<QTcpSocket*>(QString(), Qt::FindDirectChildrenOnly);
    for (QTcpSocket* s : pendingSocks) {
        s->abort();
        s->deleteLater();
    }

    QList<QThread*> threads;
    threads.reserve(tunnelChannels.size());

    for (auto it = tunnelChannels.begin(); it != tunnelChannels.end(); ++it) {
        if (it->worker)
            QMetaObject::invokeMethod(it->worker, "stop", Qt::QueuedConnection);
        if (it->thread)
            threads.append(it->thread);
    }

    for (QThread* thread : threads) {
        if (thread && thread->isRunning())
            thread->wait(5000);
    }

    tunnelChannels.clear();
}

void TunnelEndpoint::startWorker(QTcpSocket* clientSock, const QJsonObject& otpData, qint64 channelId)
{
    if (!profile || !clientSock) {
        if (clientSock)
            clientSock->deleteLater();
        return;
    }

    clientSock->setParent(this);
    clientSock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    const quint64 gen = endpointGeneration;

    QPointer<TunnelEndpoint> self = this;
    QPointer<QTcpSocket> sock = clientSock;

    HttpReqGetOTPAsync("channel_tunnel", otpData, *profile, [self, sock, channelId, gen](bool success, const QString& message, const QJsonObject& response) {
            Q_UNUSED(message);

            if (!sock)
                return;

            if (!self || gen != self->endpointGeneration) {
                sock->deleteLater();
                return;
            }

            if (!success || !response.value(QStringLiteral("ok")).toBool()) {
                sock->deleteLater();
                return;
            }

            const QString otp = response.value(QStringLiteral("message")).toString();
            if (otp.isEmpty()) {
                sock->deleteLater();
                return;
            }

            self->launchChannelWorker(sock.data(), otp, channelId);
        });
}

void TunnelEndpoint::launchChannelWorker(QTcpSocket* clientSock, const QString& otp, qint64 channelId)
{
    if (!clientSock)
        return;

    QThread* thread = new QThread;
    TunnelWorker* worker = new TunnelWorker(clientSock, otp, this->wsUrl);

    clientSock->setParent(nullptr);
    clientSock->moveToThread(thread);

    worker->moveToThread(thread);
    clientSock->setParent(worker);

    connect(thread, &QThread::started, worker, &TunnelWorker::start);
    connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
    connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &TunnelWorker::finished, this, [this, channelId]() {
        tunnelChannels.remove(channelId);
    });

    tunnelChannels[channelId] = {thread, worker, channelId};
    thread->start();
}

void TunnelEndpoint::onStartLpfChannel()
{
    if (this->tunnelId == 0)
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        if (!clientSock)
            continue;

        qint64 channelId = 0;
        while (channelId == 0)
            channelId = static_cast<qint64>(QRandomGenerator::global()->generate64() & 0x7fffffffffffffffLL);

        QJsonObject otpData;
        otpData["tunnel_id"]  = toJsonI64(this->tunnelId);
        otpData["channel_id"] = toJsonI64(channelId);

        startWorker(clientSock, otpData, channelId);
    }
}

void TunnelEndpoint::startHandshakeWorker(QTcpSocket* clientSock, const QString& type)
{
    if (!profile || !clientSock) {
        if (clientSock)
            clientSock->deleteLater();
        return;
    }

    QThread* thread = new QThread;
    auto* worker = new SocksHandshakeWorker(clientSock, this->tunnelId, type, this->useAuth, this->username, this->password, *this->profile, this->wsUrl);

    clientSock->setParent(nullptr);
    clientSock->moveToThread(thread);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &SocksHandshakeWorker::process);
    connect(worker, &SocksHandshakeWorker::workerReady, this, &TunnelEndpoint::onWorkerReady, Qt::QueuedConnection);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, this, &TunnelEndpoint::onHandshakeFailed, Qt::QueuedConnection);
    connect(worker, &SocksHandshakeWorker::workerReady, worker, &SocksHandshakeWorker::deleteLater);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, worker, &SocksHandshakeWorker::deleteLater);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void TunnelEndpoint::onStartSocksChannel()
{
    if (this->tunnelId == 0)
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        if (!clientSock)
            continue;
        startHandshakeWorker(clientSock, this->tunnelType);
    }
}

void TunnelEndpoint::onWorkerReady(TunnelWorker* worker, qint64 channelId)
{
    if (!worker || channelId == 0) {
        if (worker)
            worker->deleteLater();
        return;
    }

    QThread* thread = worker->thread();
    if (!thread) {
        worker->deleteLater();
        return;
    }

    connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
    connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &TunnelWorker::finished, this, [this, channelId]() {
        tunnelChannels.remove(channelId);
    });

    tunnelChannels[channelId] = {thread, worker, channelId};

    QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection);
}

void TunnelEndpoint::onHandshakeFailed()
{
}