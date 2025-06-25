#include <Workers/TunnelWorker.h>
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

    if (type == "socks5") {
        this->useAuth = obj["use_auth"].toBool();
        this->username = obj["username"].toString();
        this->password = obj["password"].toString();
        if (this->useAuth)
            connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocks5AuthChannel);
        else
            connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocks5Channel);
        return Listen(obj);
    }
    else if (type == "socks4") {
        connect(tcpServer, &QTcpServer::newConnection, this, &TunnelEndpoint::onStartSocks4Channel);
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
 //    auto it = tunnelChannels.find(channelId);
 //    if (it == tunnelChannels.end())
 //        return;
 //
 //    ChannelHandle handle = it.value();
 //    tunnelChannels.erase(it);
 //
	// handle.worker->stop();
 //   dQMetaObject::invokeMethod(handle.worker, "stop", Qt::QueuedConnection);
 //
 //    handle.thread->quit();
 //    handle.thread->wait(1000);
}

void TunnelEndpoint::Stop()
{
    if (tcpServer->isListening())
        tcpServer->close();

	// for (auto id : tunnelChannels.keys()) {
	// 	auto handle = tunnelChannels.value(id);
	// 	tunnelChannels.remove(id);
	//
	// 	if (handle.worker && handle.thread) {
	// 		// handle.worker->stop();
	// 		QMetaObject::invokeMethod( handle.worker, "stop", Qt::BlockingQueuedConnection );
	// 		handle.thread->quit();
	// 		handle.thread->wait();
	// 	}
	//
	// 	if (handle.worker)
	// 		handle.worker->deleteLater();
	// 	if (handle.thread)
	// 		handle.thread->deleteLater();
	// }
}

void TunnelEndpoint::onStartLpfChannel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();

        QString channelId = GenerateRandomString(8, "hex");
        QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId).arg(channelId).arg("").arg("").arg("").toUtf8().toBase64();

        QThread* thread = new QThread;
        TunnelWorker* worker = new TunnelWorker(clientSock, this->profile->GetAccessToken(), this->wsUrl, tunnelData);

        clientSock->setParent(nullptr);
        clientSock->moveToThread(thread);

        worker->moveToThread(thread);
        clientSock->setParent(worker);

        connect(thread, &QThread::started,       worker, &TunnelWorker::start);
        connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
        connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
        connect(thread, &QThread::finished,      thread, &QThread::deleteLater);
    	connect(worker, &TunnelWorker::finished, this, [this, channelId]() {StopChannel(channelId);});

        // tunnelChannels.insert(channelId, { thread, worker, channelId });
        thread->start();
    }
}

void TunnelEndpoint::onStartSocks4Channel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();

        if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 8) {
            clientSock->disconnectFromHost();
            return;
        }
        QByteArray header = clientSock->read(8);
        const uchar* d = reinterpret_cast<const uchar*>(header.constData());
        if (d[0] != 0x04 || d[1] != 0x01) {  // VER=4, CMD=1 (CONNECT)
            clientSock->disconnectFromHost();
            return;
        }

        int tPort = (static_cast<quint16>(d[2]) << 8) | static_cast<quint16>(d[3]);
        QHostAddress dstIp((static_cast<quint32>(d[4]) << 24) | (static_cast<quint32>(d[5]) << 16) | (static_cast<quint32>(d[6]) <<  8) | static_cast<quint32>(d[7]));
        QString tHost = dstIp.toString();

        QByteArray data("\x00\x5a\x00\x00\x00\x00\x00\x00", 8);
        clientSock->write(data);

        QString channelId = GenerateRandomString(8, "hex");
        QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId).arg(channelId).arg("").arg(tHost).arg(tPort).toUtf8().toBase64();

        QThread* thread = new QThread;
        TunnelWorker* worker = new TunnelWorker(clientSock, this->profile->GetAccessToken(), this->wsUrl, tunnelData);

        clientSock->setParent(nullptr);
        clientSock->moveToThread(thread);

        worker->moveToThread(thread);
        clientSock->setParent(worker);

        connect(thread, &QThread::started,       worker, &TunnelWorker::start);
        connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
        connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
        connect(thread, &QThread::finished,      thread, &QThread::deleteLater);
    	connect(worker, &TunnelWorker::finished, this, [this, channelId]() {StopChannel(channelId);});

        thread->start();
    }
}

void TunnelEndpoint::onStartSocks5Channel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
		QTcpSocket* clientSock = tcpServer->nextPendingConnection();

    	if (!clientSock->waitForReadyRead(3000)) {
			clientSock->disconnectFromHost();
			return;
		}

	  	QByteArray greeting = clientSock->readAll();
  		if (greeting.size() < 2 || static_cast<uchar>(greeting[0]) != 0x05) {
  			clientSock->disconnectFromHost();
	  		return;
  		}

	  	QByteArray response("\x05\x00", 2);
  		clientSock->write(response);

  		if (!clientSock->waitForReadyRead(3000)) {
	 		clientSock->disconnectFromHost();
			return;
  		}

    	QByteArray request = clientSock->readAll();
    	if (request.size() < 7) {
	    	clientSock->disconnectFromHost();
			continue;
    	}

    	QString mode = "tcp";
    	if (static_cast<uchar>(request[1]) == 3)
    		mode = "udp";

    	uchar addrType = static_cast<uchar>(request[3]);
    	QString dstAddress;
    	quint16 dstPort;

    	if (addrType == 0x01) { // IPv4
    		if (request.size() < 10) {
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		QHostAddress ip((static_cast<uchar>(request[4]) << 24) | (static_cast<uchar>(request[5]) << 16) | (static_cast<uchar>(request[6]) <<  8) | static_cast<uchar>(request[7]));
    		dstAddress = ip.toString();
    		dstPort = (static_cast<uchar>(request[8]) << 8) | static_cast<uchar>(request[9]);

    	} else if (addrType == 0x03) { // DNS
    		uchar domainLen = static_cast<uchar>(request[4]);
    		if (request.size() < 5 + domainLen + 2) {
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		dstAddress = QString::fromUtf8(request.mid(5, domainLen));
    		dstPort = (static_cast<uchar>(request[5 + domainLen]) << 8) | static_cast<uchar>(request[6 + domainLen]);

    	} else {
	    	clientSock->disconnectFromHost();
			continue;
    	}

	  	QByteArray data("\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00", 10);
  		clientSock->write(data);

    	QString channelId = GenerateRandomString(8, "hex");
    	QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId).arg(channelId).arg(mode).arg(dstAddress).arg(dstPort).toUtf8().toBase64();

		QThread* thread = new QThread;
	    TunnelWorker* worker = new TunnelWorker(clientSock, this->profile->GetAccessToken(), this->wsUrl, tunnelData);

        clientSock->setParent(nullptr);
        clientSock->moveToThread(thread);

        worker->moveToThread(thread);
        clientSock->setParent(worker);

        connect(thread, &QThread::started, worker, &TunnelWorker::start);
        connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
        connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    	connect(worker, &TunnelWorker::finished, this, [this, channelId]() {StopChannel(channelId);});

        thread->start();
    }
}

void TunnelEndpoint::onStartSocks5AuthChannel()
{
    if (this->tunnelId.isEmpty())
        return;

    while (tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSock = tcpServer->nextPendingConnection();

	    if (!clientSock->waitForReadyRead(3000)) {
			clientSock->disconnectFromHost();
			return;
		}

    	QByteArray greeting = clientSock->readAll();
    	if (greeting.size() < 2 || static_cast<uchar>(greeting[0]) != 0x05) {
    		clientSock->disconnectFromHost();
    		return;
    	}

    	uchar nMethods = static_cast<uchar>(greeting[1]);
    	if (greeting.size() < 2 + nMethods) {
    		clientSock->disconnectFromHost();
    		return;
    	}

    	QByteArray methods = greeting.mid(2, nMethods);
		if (!methods.contains(0x02)) {
			QByteArray response("\x05\xff", 2);
			clientSock->write(response);
    		clientSock->disconnectFromHost();
    		return;
		}

    	QByteArray response("\x05\x02", 2);
    	clientSock->write(response);

    	if (!clientSock->waitForReadyRead(3000)) {
	    	clientSock->disconnectFromHost();
			return;
    	}

    	QByteArray authRequest = clientSock->readAll();
    	if (authRequest.size() < 2 || static_cast<uchar>(authRequest[0]) != 0x01) {
    		clientSock->disconnectFromHost();
    		return;
    	}

    	uchar ulen = static_cast<uchar>(authRequest[1]);
    	if (authRequest.size() < 2 + ulen + 1) {
    		clientSock->disconnectFromHost();
    		return;
    	}
    	QString username = QString::fromUtf8(authRequest.mid(2, ulen));

    	uchar plen = static_cast<uchar>(authRequest[2 + ulen]);
    	if (authRequest.size() < 2 + ulen + 1 + plen) {
    		clientSock->disconnectFromHost();
    		return;
    	}
    	QString password = QString::fromUtf8(authRequest.mid(3 + ulen, plen));

		if (username != this->username || password != this->password) {
			QByteArray authResponse("\x01\x01", 2);
			clientSock->write(authResponse);
			clientSock->disconnectFromHost();
			return;
		}

    	QByteArray authResponse("\x01\x00", 2);
    	clientSock->write(authResponse);

    	if (!clientSock->waitForReadyRead(3000)) {
	    	clientSock->disconnectFromHost();
			return;
    	}

    	QByteArray request = clientSock->readAll();
    	if (request.size() < 7) {
    		clientSock->disconnectFromHost();
    		return;
    	}

    	QString mode = "tcp";
    	if (static_cast<uchar>(request[1]) == 3)
    		mode = "udp";

    	uchar addrType = static_cast<uchar>(request[3]);
    	QString dstAddress;
    	quint16 dstPort;

    	if (addrType == 0x01) { // IPv4
    		if (request.size() < 10) {
    			clientSock->disconnectFromHost();
		    	continue;
    		}

    		QHostAddress ip((static_cast<uchar>(request[4]) << 24) | (static_cast<uchar>(request[5]) << 16) | (static_cast<uchar>(request[6]) <<  8) | static_cast<uchar>(request[7]));
    		dstAddress = ip.toString();
    		dstPort = (static_cast<uchar>(request[8]) << 8) | static_cast<uchar>(request[9]);

    	} else if (addrType == 0x03) { // DNS
    		uchar domainLen = static_cast<uchar>(request[4]);
    		if (request.size() < 5 + domainLen + 2) {
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		dstAddress = QString::fromUtf8(request.mid(5, domainLen));
    		dstPort = (static_cast<uchar>(request[5 + domainLen]) << 8) | static_cast<uchar>(request[6 + domainLen]);
    	} else {
	    	clientSock->disconnectFromHost();
			continue;
    	}

    	QByteArray data("\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00", 10);
    	clientSock->write(data);

	    QString channelId = GenerateRandomString(8, "hex");
	    QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId).arg(channelId).arg(mode).arg(dstAddress).arg(dstPort).toUtf8().toBase64();

        QThread* thread = new QThread;
        TunnelWorker* worker = new TunnelWorker(clientSock, this->profile->GetAccessToken(), this->wsUrl, tunnelData);

        clientSock->setParent(nullptr);
        clientSock->moveToThread(thread);

        worker->moveToThread(thread);
        clientSock->setParent(worker);

        connect(thread, &QThread::started, worker, &TunnelWorker::start);
        connect(worker, &TunnelWorker::finished, thread, &QThread::quit);
        connect(worker, &TunnelWorker::finished, worker, &TunnelWorker::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    	connect(worker, &TunnelWorker::finished, this, [this, channelId]() {StopChannel(channelId);});

        thread->start();
    }
}
