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
    /*
    auto it = tunnelChannels.find(channelId);
    if (it == tunnelChannels.end())
        return;

    ChannelHandle handle = it.value();
    tunnelChannels.erase(it);

	handle.worker->stop();
   dQMetaObject::invokeMethod(handle.worker, "stop", Qt::QueuedConnection);

    handle.thread->quit();
    handle.thread->wait(1000);
    */
}

void TunnelEndpoint::Stop()
{
    if (tcpServer->isListening())
        tcpServer->close();

	/*
	for (auto id : tunnelChannels.keys()) {
		auto handle = tunnelChannels.value(id);
		tunnelChannels.remove(id);

		if (handle.worker && handle.thread) {
			// handle.worker->stop();
			QMetaObject::invokeMethod( handle.worker, "stop", Qt::BlockingQueuedConnection );
			handle.thread->quit();
			handle.thread->wait();
		}

		if (handle.worker)
			handle.worker->deleteLater();
		if (handle.thread)
			handle.thread->deleteLater();
	}
	*/
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
        	clientSock->write(QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00"), 8); // socks4 error
            clientSock->disconnectFromHost();
            continue;
        }
        QByteArray bufArray = clientSock->read(8);
        const uchar* buf = reinterpret_cast<const uchar*>(bufArray.constData());
        if (buf[0] != 0x04 || buf[1] != 0x01) {  /// VER=4, CMD=1 (CONNECT)
        	clientSock->write(QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00"), 8); // socks4 error
            clientSock->disconnectFromHost();
            continue;
        }

        int tPort = (static_cast<quint16>(buf[2]) << 8) | static_cast<quint16>(buf[3]);
        QHostAddress dstIp((static_cast<quint32>(buf[4]) << 24) | (static_cast<quint32>(buf[5]) << 16) | (static_cast<quint32>(buf[6]) <<  8) | static_cast<quint32>(buf[7]));
        QString tHost = dstIp.toString();

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

    	if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 2) {
    		clientSock->write(QByteArray("\x00\x01\x00"), 3); // invalid version of socks proxy
    		clientSock->disconnectFromHost();
			continue;
		}

	  	QByteArray buf = clientSock->read(2);
  		if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x05) {
    		clientSock->write(QByteArray("\x00\x01\x00"), 3); // invalid version of socks proxy
			clientSock->disconnectFromHost();
	  		continue;
  		}

    	uchar socksAuthCount = static_cast<uchar>(buf[1]);
    	buf = clientSock->read(socksAuthCount);
    	if (buf.size() != socksAuthCount) {
    		clientSock->write(QByteArray("\x05\xFF\x00"), 3); // no supported authentication method
    		clientSock->disconnectFromHost();
    		continue;
    	}

    	if (this->useAuth) {
    		if ( !buf.contains(0x02) ) {
    			clientSock->write(QByteArray("\x05\xFF\x00"), 3); // no supported authentication method
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		clientSock->write(QByteArray("\x05\x02"), 2); // version 5, auth user:pass

    		/// get username

    		if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 2) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		buf = clientSock->read(2);
    		if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x01) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
				clientSock->disconnectFromHost();
    			continue;
    		}
    		uchar usernameLen = static_cast<uchar>(buf[1]);
    		buf = clientSock->read(usernameLen);
    		if (buf.size() != usernameLen) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		QString username = QString::fromUtf8(buf);

			/// get password
    		buf = clientSock->read(1);
    		if (buf.size() != 1) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		uchar passwordLen = static_cast<uchar>(buf[0]);
    		buf = clientSock->read(passwordLen);
    		if (buf.size() != passwordLen) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		QString password = QString::fromUtf8(buf);

    		if (username != this->username || password != this->password) {
    			clientSock->write(QByteArray("\x01\x01"), 2); // authentication failed
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		clientSock->write(QByteArray("\x01\x00"), 2); // auth success
    	}
    	else {
    		if ( !buf.contains(0x00) ) {
    			clientSock->write(QByteArray("\x05\xFF\x00"), 3); // no supported authentication method
    			clientSock->disconnectFromHost();
    			continue;
    		}
    		clientSock->write(QByteArray("\x05\x00"), 2); // version 5, without auth
    	}

    	/// Check command: CONNECT (1)

    	if (!clientSock->waitForReadyRead(3000)) {
    		clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    		clientSock->disconnectFromHost();
    		continue;
    	}

    	buf = clientSock->read(4);
    	if (buf.size() != 4 || static_cast<uchar>(buf[0]) != 0x05 || static_cast<uchar>(buf[1]) != 0x01) {
    		clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    		clientSock->disconnectFromHost();
    		continue;
    	}
    	uchar addrType = static_cast<uchar>(buf[3]);

    	QString mode = "tcp";
    	QString dstAddress;

    	switch (addrType) {
    		case 0x01: { /// IPv4
    			buf = clientSock->read(4);
    			if (buf.size() != 4) {
    				clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    				clientSock->disconnectFromHost();
    				continue;
    			}
    			QHostAddress ip((static_cast<uchar>(buf[0]) << 24) | (static_cast<uchar>(buf[1]) << 16) | (static_cast<uchar>(buf[2]) <<  8) | static_cast<uchar>(buf[3]));
    			dstAddress = ip.toString();
    			break;
    		}
    		case 0x03: { /// DNS
    			buf = clientSock->read(1);
    			if (buf.size() != 1) {
    				clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    				clientSock->disconnectFromHost();
    				continue;
    			}
    			uchar domainLen = static_cast<uchar>(buf[0]);
    			buf = clientSock->read(domainLen);
    			if (buf.size() != domainLen) {
    				clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    				clientSock->disconnectFromHost();
    				continue;
    			}
    			dstAddress = QString::fromUtf8(buf);
    			break;
    		}
    		case 0x04: { /// IPv6
    			buf = clientSock->read(16);
    			if (buf.size() != 16) {
    				clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    				clientSock->disconnectFromHost();
    				continue;
    			}
    			QHostAddress ip(buf);
    			dstAddress = ip.toString();
    			break;
    		}
    		default: {
    			clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    			clientSock->disconnectFromHost();
    			continue;
    		}
    	}

    	buf = clientSock->read(2);
    	if (buf.size() != 2) {
    		clientSock->write(QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00"), 10); // command not supported
    		clientSock->disconnectFromHost();
    		continue;
    	}
    	quint16 dstPort = (static_cast<uchar>(buf[0]) << 8) | static_cast<uchar>(buf[1]);

    	QString channelId = GenerateRandomString(8, "hex");
    	QString tunnelData = QString("%1|%2|%3|%4|%5").arg(this->tunnelId).arg(channelId).arg(mode).arg(dstAddress).arg(dstPort).toUtf8().toBase64();

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