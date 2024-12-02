#include <Client/Requestor.h>

QJsonObject HttpReq( QString sUrl, QByteArray jsonData, QString token )
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QUrl url(sUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if( !token.isEmpty() ){
        QString bearerToken = "Bearer " + token;
        request.setRawHeader("Authorization", bearerToken.toUtf8());
    }

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
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

/// LISTENER

bool HttpReqListenerStart(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok )
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

bool HttpReqListenerEdit(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok )
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

bool HttpReqListenerStop( QString listenerName, QString listenerType, AuthProfile profile, QString* message, bool* ok )
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

/// AGENT

bool HttpReqAgentCommand( QString agentName, QString agentId, QString cmdLine, QString data, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["name"]    = agentName;
    dataJson["id"]      = agentId;
    dataJson["cmdline"] = cmdLine;
    dataJson["data"]    = data;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/agent/command";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqAgentRemove( QString agentId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["id"] = agentId;
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

bool HttpReqAgentSetTag( QString agentId, QString tag, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["id"] = agentId;
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

/// BROWSER

bool HttpReqBrowserDownload( QString action, QString fileId, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["action"] = action;
    dataJson["file"] = fileId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/download/state";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqBrowserDownloadStart( QString agentId, QString path, AuthProfile profile, QString* message, bool* ok )
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["path"] = path;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile.GetURL() + "/browser/download/start";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, profile.GetAccessToken());
    if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
        *message = jsonObject["message"].toString();
        *ok = jsonObject["ok"].toBool();
        return true;
    }
    return false;
}

bool HttpReqBrowserDisks( QString agentId, AuthProfile profile, QString* message, bool* ok )
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

bool HttpReqBrowserList( QString agentId, QString path, AuthProfile profile, QString* message, bool* ok )
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

bool HttpReqBrowserUpload( QString agentId, QString path, QString content, AuthProfile profile, QString* message, bool* ok )
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