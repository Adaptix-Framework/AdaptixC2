#include <Workers/SocksHandshakeWorker.h>
#include <Workers/TunnelWorker.h>
#include <Client/Requestor.h>
#include <QRandomGenerator>

static QByteArray readExact(QTcpSocket* sock, int n) {
    QByteArray result;
    while (result.size() < n) {
        if (sock->bytesAvailable() == 0) {
            if (!sock->waitForReadyRead(3000))
                return QByteArray();
        }
        QByteArray chunk = sock->read(n - result.size());
        if (chunk.isEmpty())
            return QByteArray();
        result += chunk;
    }
    return result;
}

SocksHandshakeWorker::SocksHandshakeWorker(QTcpSocket* sock, qint64 tunnelId, const QString& type, bool useAuth, const QString& username, const QString& password, const AuthProfile& profile, const QUrl& wsUrl)
    : clientSock(sock), tunnelId(tunnelId), tunnelType(type), useAuth(useAuth), username(username), password(password), profile(profile), wsUrl(wsUrl)
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
    qint64 channelId = 0;
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
    bool ok = false;
    if (!HttpReqGetOTP("channel_tunnel", otpData, profile, &otp, &ok) || !ok) {
        if (clientSock)
            clientSock->deleteLater();
        Q_EMIT handshakeFailed();
        return;
    }

    TunnelWorker* worker = new TunnelWorker(clientSock, otp, wsUrl);
    clientSock->setParent(worker);

    Q_EMIT workerReady(worker, channelId);
}

bool SocksHandshakeWorker::processSocks4(QJsonObject& otpData, qint64& channelId)
{
    if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 8) {
        rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
        return false;
    }

    QByteArray bufArray = readExact(clientSock, 8);
    if (bufArray.size() != 8) {
        rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
        return false;
    }
    const uchar* buf = reinterpret_cast<const uchar*>(bufArray.constData());
    if (buf[0] != 0x04 || buf[1] != 0x01) {
        rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
        return false;
    }

    const int tPort = (static_cast<quint16>(buf[2]) << 8) | static_cast<quint16>(buf[3]);
    const quint32 ipRaw = (static_cast<quint32>(buf[4]) << 24) | (static_cast<quint32>(buf[5]) << 16) | (static_cast<quint32>(buf[6]) << 8) | static_cast<quint32>(buf[7]);
    QHostAddress dstIp(ipRaw);

    QByteArray userId;
    while (true) {
        QByteArray b = readExact(clientSock, 1);
        if (b.isEmpty()) {
            rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
            return false;
        }
        if (static_cast<uchar>(b[0]) == 0x00)
            break;
        userId.append(b);
        if (userId.size() > 256) {
            rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
            return false;
        }
    }
    Q_UNUSED(userId);

    QString host = dstIp.toString();
    if (buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] != 0) {
        QByteArray domain;
        while (true) {
            QByteArray b = readExact(clientSock, 1);
            if (b.isEmpty()) {
                rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
                return false;
            }
            if (static_cast<uchar>(b[0]) == 0x00)
                break;
            domain.append(b);
            if (domain.size() > 255) {
                rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
                return false;
            }
        }
        if (domain.isEmpty()) {
            rejectAndClose(clientSock, QByteArray("\x00\x5b\x00\x00\x00\x00\x00\x00", 8));
            return false;
        }
        host = QString::fromUtf8(domain);
    }

    channelId = static_cast<qint64>(QRandomGenerator::global()->generate());

    otpData["tunnel_id"]  = toJsonI64(tunnelId);
    otpData["channel_id"] = toJsonI64(channelId);
    otpData["host"]       = host;
    otpData["port"]       = QString::number(tPort);

    return true;
}

bool SocksHandshakeWorker::processSocks5(QJsonObject& otpData, qint64& channelId)
{
    if (!clientSock->waitForReadyRead(3000) || clientSock->bytesAvailable() < 2) {
        rejectAndClose(clientSock, QByteArray("\x00\x01\x00", 3));
        return false;
    }

    QByteArray buf = readExact(clientSock, 2);
    if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x05) {
        rejectAndClose(clientSock, QByteArray("\x00\x01\x00", 3));
        return false;
    }

    uchar socksAuthCount = static_cast<uchar>(buf[1]);
    buf = readExact(clientSock, socksAuthCount);
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
        buf = readExact(clientSock, 2);
        if (buf.size() != 2 || static_cast<uchar>(buf[0]) != 0x01) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        uchar usernameLen = static_cast<uchar>(buf[1]);
        buf = readExact(clientSock, usernameLen);
        if (buf.size() != usernameLen) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        QString recvUsername = QString::fromUtf8(buf);

        buf = readExact(clientSock, 1);
        if (buf.size() != 1) {
            rejectAndClose(clientSock, QByteArray("\x01\x01", 2));
            return false;
        }
        uchar passwordLen = static_cast<uchar>(buf[0]);
        buf = readExact(clientSock, passwordLen);
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

    buf = readExact(clientSock, 4);
    if (buf.size() != 4 || static_cast<uchar>(buf[0]) != 0x05) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }
    const uchar cmd = static_cast<uchar>(buf[1]);
    if (cmd != 0x01 && cmd != 0x02) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }
    uchar addrType = static_cast<uchar>(buf[3]);

    QString mode = (cmd == 0x02) ? QStringLiteral("bind") : QStringLiteral("tcp");
    QString dstAddress;

    switch (addrType) {
        case 0x01: {
            buf = readExact(clientSock, 4);
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
            buf = readExact(clientSock, 1);
            if (buf.size() != 1) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            uchar domainLen = static_cast<uchar>(buf[0]);
            buf = readExact(clientSock, domainLen);
            if (buf.size() != domainLen) {
                rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
                return false;
            }
            dstAddress = QString::fromUtf8(buf);
            break;
        }
        case 0x04: {
            buf = readExact(clientSock, 16);
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

    buf = readExact(clientSock, 2);
    if (buf.size() != 2) {
        rejectAndClose(clientSock, QByteArray("\x05\x07\x00\x01\x00\x00\x00\x00\x00\x00", 10));
        return false;
    }
    quint16 dstPort = (static_cast<uchar>(buf[0]) << 8) | static_cast<uchar>(buf[1]);

    channelId = static_cast<qint64>(QRandomGenerator::global()->generate());

    otpData["tunnel_id"]  = toJsonI64(tunnelId);
    otpData["channel_id"] = toJsonI64(channelId);
    otpData["mode"]       = mode;
    otpData["host"]       = dstAddress;
    otpData["port"]       = QString::number(dstPort);

    return true;
}
