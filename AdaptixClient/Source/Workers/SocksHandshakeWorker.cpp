#include <Workers/SocksHandshakeWorker.h>
#include <Workers/TunnelWorker.h>
#include <Client/Requestor.h>

SocksHandshakeWorker::SocksHandshakeWorker(QTcpSocket* sock, const QString& tunnelId, const QString& type, bool useAuth, const QString& username, const QString& password, const QString& accessToken, const QString& baseUrl, const QUrl& wsUrl)
    : clientSock(sock), tunnelId(tunnelId), tunnelType(type), useAuth(useAuth), username(username), password(password), accessToken(accessToken), baseUrl(baseUrl), wsUrl(wsUrl)
{
}

SocksHandshakeWorker::~SocksHandshakeWorker() = default;

void SocksHandshakeWorker::rejectAndClose(QTcpSocket* sock, const QByteArray& response)
{
    sock->write(response);
    sock->flush();
    sock->disconnectFromHost();
}

void SocksHandshakeWorker::process()
{
    QJsonObject otpData;
    QString channelId;
    bool success = false;

    if (tunnelType == "socks4") {
        success = processSocks4(otpData, channelId);
    } else if (tunnelType == "socks5") {
        success = processSocks5(otpData, channelId);
    }

    if (!success) {
        if (clientSock) {
            clientSock->deleteLater();
        }
        Q_EMIT handshakeFailed();
        return;
    }

    QString otp;
    bool otpResult = HttpReqGetOTP("channel_tunnel", otpData, baseUrl, accessToken, &otp);
    if (!otpResult) {
        if (clientSock)
            clientSock->deleteLater();
        Q_EMIT handshakeFailed();
        return;
    }

    TunnelWorker* worker = new TunnelWorker(clientSock, otp, wsUrl);
    clientSock->setParent(worker);

    Q_EMIT workerReady(worker, channelId);
}

bool SocksHandshakeWorker::processSocks4(QJsonObject& otpData, QString& channelId)
{
    if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 8) {
        rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
        return false;
    }

    QByteArray bufArray = clientSock->read(8);
    const uchar* buf = reinterpret_cast<const uchar*>(bufArray.constData());
    if (buf[0] != 0x04 || buf[1] != 0x01) {
        rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
        return false;
    }

    int tPort = (static_cast<quint16>(buf[2]) << 8) | static_cast<quint16>(buf[3]);
    QHostAddress dstIp((static_cast<quint32>(buf[4]) << 24) | (static_cast<quint32>(buf[5]) << 16) |
                       (static_cast<quint32>(buf[6]) << 8) | static_cast<quint32>(buf[7]));

    channelId = GenerateRandomString(8, "hex");

    otpData["tunnel_id"]  = tunnelId;
    otpData["channel_id"] = channelId;
    otpData["host"]       = dstIp.toString();
    otpData["port"]       = QString::number(tPort);

    return true;
}

bool SocksHandshakeWorker::processSocks5(QJsonObject& otpData, QString& channelId)
{
    if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 2) {
        rejectAndClose(clientSock, QByteArray("\x00\x01\x00", 3));
        return false;
    }

    QByteArray buf = clientSock->read(2);
    if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x05) {
        rejectAndClose(clientSock, QByteArray("\x00\x01\x00", 3));
        return false;
    }

    uchar socksAuthCount = static_cast<uchar>(buf[1]);
    buf = clientSock->read(socksAuthCount);
    if (buf.size() != socksAuthCount) {
        rejectAndClose(clientSock, QByteArray("\x05\xFF\x00", 3));
        return false;
    }

    if (useAuth) {
        if (!buf.contains(0x02)) {
            rejectAndClose(clientSock, QByteArray("\x05\xFF\x00", 3));
            return false;
        }
        clientSock->write(QByteArray("\x05\x02", 2));
        clientSock->flush();

        if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 2) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        buf = clientSock->read(2);
        if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x01) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        uchar usernameLen = static_cast<uchar>(buf[1]);
        buf = clientSock->read(usernameLen);
        if (buf.size() != usernameLen) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        QString recvUsername = QString::fromUtf8(buf);

        buf = clientSock->read(1);
        if (buf.size() != 1) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        uchar passwordLen = static_cast<uchar>(buf[0]);
        buf = clientSock->read(passwordLen);
        if (buf.size() != passwordLen) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        QString recvPassword = QString::fromUtf8(buf);

        if (recvUsername != username || recvPassword != password) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        clientSock->write(QByteArray("\x01\x00", 2));
        clientSock->flush();
    }
    else {
        if (!buf.contains(0x00)) {
            rejectAndClose(clientSock, QByteArray("\x05\xFF\x00", 3));
            return false;
        }
        clientSock->write(QByteArray("\x05\x00", 2));
        clientSock->flush();
    }

    if (!clientSock->waitForReadyRead(3000)) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }

    buf = clientSock->read(4);
    if (buf.size() != 4 || static_cast<uchar>(buf[0]) != 0x05 || static_cast<uchar>(buf[1]) != 0x01) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }
    uchar addrType = static_cast<uchar>(buf[3]);

    QString mode = "tcp";
    QString dstAddress;

    switch (addrType) {
        case 0x01: {
            buf = clientSock->read(4);
            if (buf.size() != 4) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            QHostAddress ip((static_cast<uchar>(buf[0]) << 24) | (static_cast<uchar>(buf[1]) << 16) |
                            (static_cast<uchar>(buf[2]) << 8) | static_cast<uchar>(buf[3]));
            dstAddress = ip.toString();
            break;
        }
        case 0x03: {
            buf = clientSock->read(1);
            if (buf.size() != 1) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            uchar domainLen = static_cast<uchar>(buf[0]);
            buf = clientSock->read(domainLen);
            if (buf.size() != domainLen) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            dstAddress = QString::fromUtf8(buf);
            break;
        }
        case 0x04: {
            buf = clientSock->read(16);
            if (buf.size() != 16) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            QHostAddress ip(buf);
            dstAddress = ip.toString();
            break;
        }
        default: {
            rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
            return false;
        }
    }

    buf = clientSock->read(2);
    if (buf.size() != 2) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }
    quint16 dstPort = (static_cast<uchar>(buf[0]) << 8) | static_cast<uchar>(buf[1]);

    channelId = GenerateRandomString(8, "hex");

    otpData["tunnel_id"]  = tunnelId;
    otpData["channel_id"] = channelId;
    otpData["mode"]       = mode;
    otpData["host"]       = dstAddress;
    otpData["port"]       = QString::number(dstPort);

    return true;
}
