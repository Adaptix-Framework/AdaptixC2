#include <Workers/TunnelWorker.h>
#include <Workers/SocksHandshakeWorker.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AuthProfile.h>

TunnelEndpoint::TunnelEndpoint(QObject* parent) : QObject(parent), tcpServer(new QTcpServer(this)){}

TunnelEndpoint::~TunnelEndpoint() = default;

bool TunnelEndpoint::StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData)
{
    this->profile = profile;

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );
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
    else if (type == "socks4") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocksChannel);
        return Listen(obj);
    }
    else if (type == "lportfwd") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartLpfChannel);
        return Listen(obj);
    }
    else if (type == "rportfwd") {

    }
    return false;
}

bool TunnelEndpoint::Listen(const QJsonObject &obj)
{
    lHost = obj["l_host"].toString();
    lPort = obj["l_port"].toInt();

    if (!tcpServer->listen(QHostAddress(lHost), lPort)) {
        MessageError(tcpServer->errorString());
        return false;
    }
    return true;
}

void TunnelEndpoint::SetTunnelId(const QString &tunnelId)
{
    this->tunnelId = tunnelId;
}

void TunnelEndpoint::StopChannel(const QString& channelId)
{
    auto it = tunnelChannels.find(channelId);
    if (it == tunnelChannels.end())
        return;

    tunnelChannels.erase(it);
}

void TunnelEndpoint::Stop()
{
    if (tcpServer && tcpServer->isListening())
        tcpServer->close();

    for (auto it = tunnelChannels.begin(); it != tunnelChannels.end(); ) {
        ChannelHandle handle = it.value();
        it = tunnelChannels.erase(it);

        if (handle.worker && handle.thread) {
            QMetaObject::invokeMethod(handle.worker, "stop", Qt::QueuedConnection);
            handle.thread->quit();
            handle.thread->wait(3000);
        }
    }
}

void TunnelEndpoint::startWorker(QTcpSocket* clientSock, const QString& tunnelData)
{
    QString channelId = tunnelData.section('|', 1, 1);

    QThread* thread = new QThread;
    TunnelWorker* worker = new TunnelWorker(clientSock, this->profile->GetAccessToken(), this->wsUrl, QString(tunnelData.toUtf8().toBase64()));

    clientSock->setParent(nullptr);
    clientSock->moveToThread(thread);

    worker->moveToThread(thread);
    clientSock->setParent(worker);

    connect(thread, &QThread::started,       worker, &TunnelWorker::start);
    connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
    connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
    connect(thread, &QThread::finished,      thread, &QThread::deleteLater);
    connect(worker, &TunnelWorker::finished, this, [this, channelId]() {StopChannel(channelId);});

    tunnelChannels[channelId] = {thread, worker, channelId};
    thread->start();
}

void TunnelEndpoint::onStartLpfChannel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();

        QString channelId = GenerateRandomString(8, "hex");
        QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId, channelId, QString(), QString(), QString());

        startWorker(clientSock, tunnelData);
    }
}

void TunnelEndpoint::startHandshakeWorker(QTcpSocket* clientSock, const QString& type)
{
    QThread* thread = new QThread;
    SocksHandshakeWorker* worker = new SocksHandshakeWorker(clientSock, this->tunnelId, type, this->useAuth, this->username, this->password, this->profile->GetAccessToken(), this->wsUrl);

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
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        startHandshakeWorker(clientSock, this->tunnelType);
    }
}

void TunnelEndpoint::onWorkerReady(TunnelWorker* worker, const QString& channelId)
{
    QThread* thread = worker->thread();

    connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
    connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(worker, &TunnelWorker::finished, this, [this, channelId]() { StopChannel(channelId); });

    tunnelChannels[channelId] = {thread, worker, channelId};

    QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection);
}

void TunnelEndpoint::onHandshakeFailed()
{
}