#include <Workers/TunnelWorker.h>
#include <Workers/SocksHandshakeWorker.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <QRandomGenerator>
#include <QPointer>
#include <QTimer>
#include <QAbstractSocket>
#include <QTcpSocket>

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
        this->fHost = obj["t_host"].toString();
        this->fPort = static_cast<quint16>(obj["t_port"].toInt());
        if (this->fHost.isEmpty() || this->fPort == 0)
            return false;
        return true;
    }
    return false;
}

namespace {
QString canonicalTunnelType(const QString& raw)
{
    const QString key = raw.trimmed().toLower();
    if (key.contains(QLatin1String("socks5")))
        return QStringLiteral("socks5");
    if (key.contains(QLatin1String("socks4")))
        return QStringLiteral("socks4");
    if (key == QLatin1String("lportfwd") || key.contains(QLatin1String("lport")) || (key.contains(QLatin1String("local")) && key.contains(QLatin1String("port"))))
        return QStringLiteral("lportfwd");
    if (key == QLatin1String("rportfwd") || key.contains(QLatin1String("rport")) || (key.contains(QLatin1String("reverse")) && key.contains(QLatin1String("port"))))
        return QStringLiteral("rportfwd");
    return key;
}
}

bool TunnelEndpoint::Restore(AuthProfile* authProfile, const TunnelData& data)
{
    if (!authProfile || data.TunnelId == 0)
        return false;

    this->profile = authProfile;
    this->tunnelId = data.TunnelId;
    this->tunnelType = canonicalTunnelType(data.Type);
    this->paused = false;

    const QString urlTemplate = QStringLiteral("wss://%1:%2%3/channel");
    this->wsUrl = QUrl(urlTemplate.arg(authProfile->GetHost()).arg(authProfile->GetPort()).arg(authProfile->GetEndpoint()));

    if (tunnelType == QLatin1String("rportfwd")) {
        this->fHost = data.Fhost.trimmed();
        this->fPort = static_cast<quint16>(data.Fport.trimmed().toUInt());
        if (this->fHost.isEmpty() || this->fPort == 0) {
            MessageError("Tunnel target address is missing");
            return false;
        }
        return true;
    }

    if (tunnelType != QLatin1String("socks4") && tunnelType != QLatin1String("socks5") && tunnelType != QLatin1String("lportfwd")) {
        MessageError("Unknown tunnel type");
        return false;
    }

    const auto slot = (tunnelType == QLatin1String("lportfwd"))
        ? &TunnelEndpoint::onStartLpfChannel
        : &TunnelEndpoint::onStartSocksChannel;
    connect(tcpServer, &QTcpServer::newConnection, this, slot);

    QString host = data.Interface.trimmed();
    if (host.isEmpty() || host == QLatin1String("*")) {
        host = QStringLiteral("0.0.0.0");
    }

    bool portOk = false;
    const int port = data.Port.trimmed().toInt(&portOk);
    if (!portOk || port <= 0 || port > 65535) {
        MessageError("Tunnel listen port is missing");
        return false;
    }

    QJsonObject obj;
    obj["l_host"] = host;
    obj["l_port"] = port;
    return Listen(obj);
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

void TunnelEndpoint::SetActive(bool active)
{
    if (active) {
        Resume();
    }
    else {
        Pause();
    }
}

void TunnelEndpoint::abortOwnedSockets()
{
    const QList<QTcpSocket*> pendingSocks = findChildren<QTcpSocket*>(QString(), Qt::FindDirectChildrenOnly);
    for (QTcpSocket* s : pendingSocks) {
        s->blockSignals(true);
        s->abort();
        s->setParent(nullptr);
        delete s;
    }
}

void TunnelEndpoint::discardWorker(TunnelWorker* worker)
{
    if (!worker)
        return;

    QThread* thread = worker->thread();
    connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
    if (thread && thread->isRunning() && thread != QThread::currentThread()) {
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
    }
    else {
        worker->stop();
    }

    if (thread && thread->isRunning() && thread != QThread::currentThread()) {
        thread->quit();
        thread->wait(1000);
    }
}

void TunnelEndpoint::Pause()
{
    if (paused)
        return;

    paused = true;
    ++endpointGeneration;

    const QList<qint64> reverseIds = pendingReverse.values();
    pendingReverse.clear();
    for (qint64 channelId : reverseIds) {
        nackReverseChannel(channelId);
    }
    if (tcpServer && tcpServer->isListening()) {
        tcpServer->close();
    }
    abortOwnedSockets();
}

void TunnelEndpoint::Resume()
{
    paused = false;
    if (tunnelType == QLatin1String("rportfwd") || lPort == 0 || !tcpServer) {
        return;
    }
    if (tcpServer->isListening()) {
        return;
    }
    if (!tcpServer->listen(QHostAddress(lHost), lPort)) {
        paused = true;
        MessageError(tcpServer->errorString());
    }
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
    paused = true;

    const QList<qint64> reverseIds = pendingReverse.values();
    pendingReverse.clear();
    for (qint64 channelId : reverseIds) {
        nackReverseChannel(channelId);
    }

    if (tcpServer && tcpServer->isListening())
        tcpServer->close();

    abortOwnedSockets();

    const QList<ChannelHandle> channels = tunnelChannels.values();
    tunnelChannels.clear();

    for (const ChannelHandle& channel : channels) {
        TunnelWorker* worker = channel.worker;
        QThread* thread = channel.thread;
        if (!worker) {
            continue;
        }
        QObject::disconnect(worker, nullptr, this, nullptr);
        if (thread && thread->isRunning() && worker->thread() == thread) {
            QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        }
    }

    for (const ChannelHandle& channel : channels) {
        TunnelWorker* worker = channel.worker;
        QThread* thread = channel.thread;
        if (worker && thread && thread->isRunning()) {
            QMetaObject::invokeMethod(worker, "deleteLater", Qt::QueuedConnection);
        }
        if (!thread || !thread->isRunning()) {
            continue;
        }
        thread->quit();
        if (!thread->wait(3000)) {
            thread->terminate();
            thread->wait(1000);
        }
    }
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
    bindChannel(thread, worker, channelId);
    thread->start();
}

void TunnelEndpoint::bindChannel(QThread* thread, TunnelWorker* worker, qint64 channelId)
{
    if (!thread || !worker) {
        return;
    }

    connect(thread, &QThread::finished, thread, &QThread::deleteLater, Qt::UniqueConnection);
    connect(worker, &TunnelWorker::finished, this, [this, channelId]() {
        auto it = tunnelChannels.find(channelId);
        if (it == tunnelChannels.end())
            return;
        QPointer<TunnelWorker> doneWorker = it->worker;
        QPointer<QThread> doneThread = it->thread;
        tunnelChannels.erase(it);
        if (doneWorker) {
            QMetaObject::invokeMethod(doneWorker, "deleteLater", Qt::QueuedConnection);
        }
        if (doneThread && doneThread->isRunning()) {
            doneThread->quit();
        }
    }, Qt::QueuedConnection);

    tunnelChannels.insert(channelId, {thread, worker, channelId});
}

void TunnelEndpoint::onStartLpfChannel()
{
    if (this->tunnelId == 0 || paused) {
        while (tcpServer && tcpServer->hasPendingConnections()) {
            if (QTcpSocket* sock = tcpServer->nextPendingConnection()) {
                sock->deleteLater();
            }
        }
        return;
    }

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        if (!clientSock)
            continue;

        qint64 channelId = 0;
        while (channelId == 0) {
            channelId = static_cast<qint64>(QRandomGenerator::global()->generate64() & 0x7fffffffffffffffLL);
        }

        QJsonObject otpData;
        otpData["tunnel_id"]  = toJsonI64(this->tunnelId);
        otpData["channel_id"] = toJsonI64(channelId);

        startWorker(clientSock, otpData, channelId);
    }
}

void TunnelEndpoint::startHandshakeWorker(QTcpSocket* clientSock, const QString& type)
{
    if (!profile || !clientSock) {
        if (clientSock) {
            clientSock->deleteLater();
        }
        return;
    }

    QThread* thread = new QThread;
    auto* worker = new SocksHandshakeWorker(clientSock, this->tunnelId, type, this->useAuth, this->username, this->password, *this->profile, this->wsUrl);

    clientSock->setParent(nullptr);
    clientSock->moveToThread(thread);
    worker->moveToThread(thread);

    const quint64 gen = endpointGeneration;
    connect(thread, &QThread::started, worker, &SocksHandshakeWorker::process);
    connect(worker, &SocksHandshakeWorker::workerReady, this, [this, gen](TunnelWorker* readyWorker, qint64 channelId) {
        if (gen != endpointGeneration || paused || !readyWorker || channelId == 0) {
            discardWorker(readyWorker);
            return;
        }
        onWorkerReady(readyWorker, channelId);
    }, Qt::QueuedConnection);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, this, &TunnelEndpoint::onHandshakeFailed, Qt::QueuedConnection);
    connect(worker, &SocksHandshakeWorker::workerReady, worker, &SocksHandshakeWorker::deleteLater);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, worker, &SocksHandshakeWorker::deleteLater);
    connect(worker, &SocksHandshakeWorker::handshakeFailed, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void TunnelEndpoint::onStartSocksChannel()
{
    if (this->tunnelId == 0 || paused) {
        while (tcpServer && tcpServer->hasPendingConnections()) {
            if (QTcpSocket* sock = tcpServer->nextPendingConnection()) {
                sock->deleteLater();
            }
        }
        return;
    }

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        if (!clientSock)
            continue;
        startHandshakeWorker(clientSock, this->tunnelType);
    }
}

void TunnelEndpoint::onWorkerReady(TunnelWorker* worker, qint64 channelId)
{
    if (!worker || channelId == 0 || paused) {
        discardWorker(worker);
        return;
    }

    QThread* thread = worker->thread();
    if (!thread) {
        worker->deleteLater();
        return;
    }

    bindChannel(thread, worker, channelId);
    QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection);
}

void TunnelEndpoint::onHandshakeFailed()
{
}

void TunnelEndpoint::nackReverseChannel(qint64 channelId)
{
    if (!profile || tunnelId == 0 || channelId == 0)
        return;
    HttpReqTunnelChannelNackAsync(tunnelId, channelId, *profile, [](bool, const QString&, const QJsonObject&) {});
}

void TunnelEndpoint::startReverseWorker(QTcpSocket* clientSock, const QJsonObject& otpData, qint64 channelId)
{
    if (!profile || !clientSock) {
        if (clientSock)
            clientSock->deleteLater();
        nackReverseChannel(channelId);
        return;
    }

    clientSock->setParent(this);
    clientSock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    const quint64 gen = endpointGeneration;

    QPointer<TunnelEndpoint> self = this;
    QPointer<QTcpSocket> sock = clientSock;

    HttpReqGetOTPAsync("channel_tunnel", otpData, *profile, [self, sock, channelId, gen](bool success, const QString& message, const QJsonObject& response) {
            Q_UNUSED(message);

            if (!self || gen != self->endpointGeneration || self->paused) {
                if (self && (gen != self->endpointGeneration || self->paused)) {
                    self->nackReverseChannel(channelId);
                }
                if (sock) {
                    sock->deleteLater();
                }
                return;
            }

            if (!sock) {
                self->nackReverseChannel(channelId);
                return;
            }

            if (!success || !response.value(QStringLiteral("ok")).toBool()) {
                sock->deleteLater();
                self->nackReverseChannel(channelId);
                return;
            }

            const QString otp = response.value(QStringLiteral("message")).toString();
            if (otp.isEmpty()) {
                sock->deleteLater();
                self->nackReverseChannel(channelId);
                return;
            }

            self->launchChannelWorker(sock.data(), otp, channelId);
        });
}

void TunnelEndpoint::onReverseAccept(qint64 channelId)
{
    if (this->tunnelId == 0 || channelId == 0 || this->fHost.isEmpty() || this->fPort == 0)
        return;
    if (paused) {
        nackReverseChannel(channelId);
        return;
    }
    if (tunnelChannels.contains(channelId))
        return;

    pendingReverse.insert(channelId);

    auto* sock = new QTcpSocket(this);
    sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    QPointer<TunnelEndpoint> self = this;
    QPointer<QTcpSocket> sockPtr = sock;
    const quint64 gen = endpointGeneration;

    QObject::connect(sock, &QTcpSocket::connected, this, [self, sockPtr, channelId, gen]() {
        if (self) {
            self->pendingReverse.remove(channelId);
        }
        if (!self || !sockPtr || gen != self->endpointGeneration || self->paused) {
            if (self) {
                self->nackReverseChannel(channelId);
            }
            if (sockPtr) {
                sockPtr->deleteLater();
            }
            return;
        }
        QJsonObject otpData;
        otpData["tunnel_id"]  = toJsonI64(self->tunnelId);
        otpData["channel_id"] = toJsonI64(channelId);
        otpData["mode"]       = QStringLiteral("reverse");
        self->startReverseWorker(sockPtr.data(), otpData, channelId);
    });

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const auto errorSignal = &QAbstractSocket::errorOccurred;
#else
    const auto errorSignal = QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error);
#endif
    QObject::connect(sock, errorSignal, this, [self, sockPtr, channelId, gen](QAbstractSocket::SocketError) {
        if (!self) {
            return;
        }
        if (gen != self->endpointGeneration || self->paused) {
            self->pendingReverse.remove(channelId);
            self->nackReverseChannel(channelId);
            if (sockPtr) {
                sockPtr->deleteLater();
            }
            return;
        }
        if (sockPtr && sockPtr->state() == QAbstractSocket::ConnectedState)
            return;
        self->pendingReverse.remove(channelId);
        self->nackReverseChannel(channelId);
        if (sockPtr)
            sockPtr->deleteLater();
    });

    QTimer::singleShot(15000, sock, [self, sockPtr, channelId, gen]() {
        if (!self || !sockPtr)
            return;
        if (gen != self->endpointGeneration || self->paused) {
            self->pendingReverse.remove(channelId);
            self->nackReverseChannel(channelId);
            sockPtr->abort();
            sockPtr->deleteLater();
            return;
        }
        if (sockPtr->state() == QAbstractSocket::ConnectedState)
            return;

        self->pendingReverse.remove(channelId);
        self->nackReverseChannel(channelId);
        sockPtr->abort();
        sockPtr->deleteLater();
    });

    sock->connectToHost(this->fHost, this->fPort);
}