#include <Client/TunnelEndpoint.h>

TunnelEndpoint::TunnelEndpoint(QObject* parent) : QObject(parent), tcpServer(new QTcpServer(this))
{
    connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartLpfChannel);
}

TunnelEndpoint::~TunnelEndpoint() = default;

bool TunnelEndpoint::StartTunnel(AuthProfile* profile, const QString &type, const QByteArray &jsonData)
{
    this->profile = profile;

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );
    this->wsUrl = QUrl(sUrl);

    if (type == "socks5") {

    }
    else if (type == "socks4") {

    }
    else if (type == "lportfwd") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartLpfChannel);
        return StartLocalPortFwd(jsonData);
    }
    else if (type == "rportfwd") {

    }
    return false;
}

bool TunnelEndpoint::StartLocalPortFwd(const QByteArray &jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject obj = doc.object();
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

void TunnelEndpoint::Stop()
{
    if (tcpServer->isListening())
        tcpServer->close();

    for (auto it = tunnelChannels.begin(); it != tunnelChannels.end(); ) {
        auto handle = it.value();
        QMetaObject::invokeMethod(handle.worker, "stop", Qt::QueuedConnection);
        handle.thread->quit();
        handle.thread->wait(1000);
        it = tunnelChannels.erase(it);
    }
}

void TunnelEndpoint::onStartLpfChannel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();
        QString channelId = GenerateRandomString(8, "hex");

        QThread* thread = new QThread;
        TunnelWorker* worker = new TunnelWorker(this->profile->GetAccessToken(), clientSock, this->wsUrl, this->tunnelId, channelId);

        clientSock->setParent(nullptr);
        clientSock->moveToThread(thread);

        worker->moveToThread(thread);
        clientSock->setParent(worker);

        connect(thread, &QThread::started, worker, &TunnelWorker::start);
        connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
        connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);

        tunnelChannels.insert(channelId, { thread, worker });
        thread->start();
    }
}

void TunnelEndpoint::onStartSocks4Channel()
{
    
}

void TunnelEndpoint::StopChannel(const QString& channelId)
{
    auto it = tunnelChannels.find(channelId);
    if (it == tunnelChannels.end())
        return;

    ChannelHandle handle = it.value();
    tunnelChannels.erase(it);

    QMetaObject::invokeMethod(handle.worker, "stop", Qt::QueuedConnection);

    handle.thread->quit();
    handle.thread->wait(1000);
}
