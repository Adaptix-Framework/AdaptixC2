#include <Client/Requestor.h>
#include <Client/AuthProfile.h>

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, const int timeout)
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QUrl url(sUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if( !token.isEmpty() ) {
        QString bearerToken = "Bearer " + token;
        request.setRawHeader("Authorization", bearerToken.toUtf8());
    }

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    QTimer timeoutTimer;
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        reply->abort();
        eventLoop.quit();
    });
    timeoutTimer.start(timeout);

    eventLoop.exec();

    QJsonObject jsonObject;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject()) {
            jsonObject = jsonResponse.object();
        }
    }
    reply->deleteLater();
    return jsonObject;
}

bool HttpReqLogin(AuthProfile* profile)
{
    QJsonObject dataJson;
    dataJson["username"] = profile->GetUsername();
    dataJson["password"] = profile->GetPassword();
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile->GetURL() + "/login";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, QString());
    if (jsonObject.contains("access_token") && jsonObject.contains("refresh_token")) {
        profile->SetAccessToken( jsonObject["access_token"].toString() );
        profile->SetRefreshToken( jsonObject["refresh_token"].toString() );
        return true;
    }
    return false;
}

bool HttpReqSync(AuthProfile profile)
{
    QJsonObject dataJson;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/sync";
    HttpReq(sUrl, jsonData, profile.GetAccessToken());
    return false;
}

bool HttpReqJwtUpdate(AuthProfile* profile)
{
    QJsonObject dataJson;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile->GetURL() + "/refresh";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile->GetRefreshToken());
    if ( jsonObject.contains("access_token") ) {
        profile->SetAccessToken( jsonObject["access_token"].toString() );
        return true;
    }
    return false;
}

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["id"]   = objectId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/otp/generate";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///LISTENER

bool HttpReqListenerStart(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/listener/create";
    QJsonObject jsonObject = HttpReq( sUrl, jsonData, profile.GetAccessToken() );
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqListenerEdit(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();
    QString sUrl = profile.GetURL() + "/listener/edit";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqListenerStop(const QString &listenerName, const QString &listenerType, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/listener/stop";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///AGENT

QJsonObject HttpReqTimeout( int timeout, const QString &sUrl, const QByteArray &jsonData, const QString &token )
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QUrl url(sUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if( !token.isEmpty() ) {
        QString bearerToken = "Bearer " + token;
        request.setRawHeader("Authorization", bearerToken.toUtf8());
    }

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    QTimer timeoutTimer;
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        reply->abort();
        eventLoop.quit();
    });
    timeoutTimer.start(timeout);

    eventLoop.exec();

    QJsonObject jsonObject;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject()) {
            jsonObject = jsonResponse.object();
        }
    }
    reply->deleteLater();
    return jsonObject;
}

bool HttpReqAgentGenerate(const QString &listenerName, const QString &listenerType, const QString &agentName, const QString &configData, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["listener_name"] = listenerName;
    dataJson["listener_type"] = listenerType;
    dataJson["agent"]         = agentName;
    dataJson["config"]        = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/generate";
    QJsonObject jsonObject = HttpReqTimeout( 30000,sUrl, jsonData, profile.GetAccessToken() );
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentCommand(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok )
{
    QString sUrl = profile.GetURL() + "/agent/command/execute";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqConsoleRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : agentsId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id_array"] = arrayId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/console/remove";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : agentsId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id_array"] = arrayId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/remove";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentSetTag( QStringList agentsId, const QString &tag, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : agentsId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id_array"] = arrayId;
    dataJson["tag"] = tag;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/settag";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentSetMark( QStringList agentsId, const QString &mark, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : agentsId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id_array"] = arrayId;
    dataJson["mark"] = mark;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/setmark";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentSetColor( QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : agentsId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id_array"] = arrayId;
    dataJson["bc"] = background;
    dataJson["fc"] = foreground;
    dataJson["reset"] = reset;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/setcolor";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///TASK

bool HttpReqTaskStop(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : tasksId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = arrayId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/task/stop";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqTasksDelete(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : tasksId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = arrayId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/task/delete";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqTasksHook(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok)
{
    QString sUrl = profile.GetURL() + "/agent/task/hook";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

/// DOWNLOADS

bool HttpReqDownloadStart(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["path"] = path;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/download/start";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqDownloadAction(const QString &action, const QString &fileId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["file_id"] = fileId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/download/" + action;
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

/// BROWSER

bool HttpReqBrowserDisks(const QString &agentId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/disks";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqBrowserProcess(const QString &agentId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/process";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqBrowserList(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["path"]     = path;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/files";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqBrowserUpload(const QString &agentId, const QString &path, const QString &content, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"]    = agentId;
    dataJson["remote_path"] = path;
    dataJson["content"]     = content;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/upload";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///TUNNEL

bool HttpReqTunnelStartServer(const QString &tunnelType, const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok)
{
    QString sUrl = profile.GetURL() + "/tunnel/start/" + tunnelType;
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqTunnelStop(const QString &tunnelId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/tunnel/stop";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqTunnelSetInfo(const QString &tunnelId, const QString &info, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    dataJson["p_info"] = info;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/tunnel/setinfo";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///SCREEN

bool HttpReqScreenSetNote( QStringList scrensId, const QString &note, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : scrensId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["screen_id_array"] = arrayId;
    dataJson["note"] = note;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/screen/setnote";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqScreenRemove( QStringList scrensId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonArray arrayId;
    for (QString item : scrensId)
        arrayId.append(item);

    QJsonObject dataJson;
    dataJson["screen_id_array"] = arrayId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/screen/remove";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

///CREDS

bool HttpReqCredentialsCreate(const QByteArray &jsonData, AuthProfile profile, QString *message, bool *ok)
{
    QString sUrl = profile.GetURL() + "/creds/add";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqCredentialsEdit(const QByteArray &jsonData, AuthProfile profile, QString *message, bool *ok)
{
    QString sUrl = profile.GetURL() + "/creds/edit";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqCredentialsRemove(const QString &credsId, AuthProfile profile, QString* message, bool* ok)
{
    QJsonObject dataJson;
    dataJson["cred_id"] = credsId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/creds/remove";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}
