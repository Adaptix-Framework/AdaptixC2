#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/HttpRequestManager.h>

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, const int timeout)
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QUrl url(sUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setSslConfiguration(sslConfig);
    if( !token.isEmpty() ) {
        QString bearerToken = "Bearer " + token;
        request.setRawHeader("Authorization", bearerToken.toUtf8());
    }

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    QTimer timeoutTimer;
    if (timeout > 0) {
        QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
            reply->abort();
            eventLoop.quit();
        });
        timeoutTimer.start(timeout);
    }
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
    dataJson["version"]  = SMALL_VERSION;
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

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok)
{
    QJsonObject innerData;
    innerData["id"] = objectId;

    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = innerData;
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

bool HttpReqGetOTP(const QString &type, const QJsonObject &data, const QString &baseUrl, const QString &accessToken, QString* otp)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = data;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = baseUrl + "/otp/generate";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, accessToken);
    if ( jsonObject.contains("ok") && jsonObject["ok"].toBool() && jsonObject.contains("message") ) {
        *otp = jsonObject["message"].toString();
        return true;
    }
    return false;
}

bool HttpReqGetOTP(const QString &type, const QString &objectId, const QString &baseUrl, const QString &accessToken, QString* otp)
{
    QJsonObject innerData;
    innerData["id"] = objectId;

    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = innerData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = baseUrl + "/otp/generate";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, accessToken);
    if ( jsonObject.contains("ok") && jsonObject["ok"].toBool() && jsonObject.contains("message") ) {
        *otp = jsonObject["message"].toString();
        return true;
    }
    return false;
}

/// ASYNC VERSIONS

void HttpReqAgentRemoveAsync(const QStringList &agentsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArray(agentsId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/remove", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqAgentSetTagAsync(const QStringList &agentsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArray(agentsId);
    dataJson["tag"] = tag;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/set/tag", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqAgentSetMarkAsync(const QStringList &agentsId, const QString &mark, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArray(agentsId);
    dataJson["mark"] = mark;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/set/mark", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqAgentSetColorAsync(const QStringList &agentsId, const QString &background, const QString &foreground, const bool reset, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArray(agentsId);
    dataJson["bc"] = background;
    dataJson["fc"] = foreground;
    dataJson["reset"] = reset;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/set/color", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqAgentUpdateDataAsync(const QString &agentId, const QJsonObject &updateData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson = updateData;
    dataJson["agent_id"] = agentId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/update/data", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqAgentGenerateAsync(const QString &listenerName, const QString &agentName, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["listener_name"] = listenerName;
    dataJson["agent"]         = agentName;
    dataJson["config"]        = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/generate", profile.GetAccessToken(), jsonData, callback, 30000);
}

void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    HttpRequestManager::instance().postFireAndForget(profile.GetURL(), "/agent/command/execute", profile.GetAccessToken(), jsonData);
}

void HttpReqAgentCommandFileAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    HttpRequestManager::instance().postFireAndForget(profile.GetURL(), "/agent/command/file", profile.GetAccessToken(), jsonData);
}

void HttpReqConsoleRemoveAsync(const QStringList &agentsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArray(agentsId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/console/remove", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTaskCancelAsync(const QString &agentId, const QStringList &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = toJsonArray(tasksId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/task/cancel", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTasksDeleteAsync(const QString &agentId, const QStringList &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = toJsonArray(tasksId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/task/delete", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/agent/task/hook", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTasksSaveAsync(const QString &agentId, const QString &CommandLine, const int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"]     = agentId;
    dataJson["command_line"] = CommandLine;
    dataJson["message_type"] = MessageType;
    dataJson["message"]      = Message;
    dataJson["clear_text"]   = ClearText;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/agent/task/save", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/creds/add", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/creds/edit", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqCredentialsRemoveAsync(const QStringList &credsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["cred_id_array"] = toJsonArray(credsId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/creds/remove", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqCredentialsSetTagAsync(const QStringList &credsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id_array"] = toJsonArray(credsId);
    dataJson["tag"] = tag;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/creds/set/tag", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/targets/add", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/targets/edit", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTargetRemoveAsync(const QStringList &targetsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["target_id_array"] = toJsonArray(targetsId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/targets/remove", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTargetSetTagAsync(const QStringList &targetsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id_array"] = toJsonArray(targetsId);
    dataJson["tag"] = tag;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/targets/set/tag", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/listener/create", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/listener/edit", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/listener/stop", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/listener/pause", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/listener/resume", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqDownloadActionAsync(const QString &action, const QString &fileId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["file_id"] = fileId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/download/" + action, profile.GetAccessToken(), jsonData, callback);
}

void HttpReqDownloadDelete(const QStringList &fileId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["file_id_array"] = toJsonArray(fileId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/download/delete", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqScreenSetNoteAsync(const QStringList &screensId, const QString &note, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["screen_id_array"] = toJsonArray(screensId);
    dataJson["note"] = note;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/screen/setnote", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqScreenRemoveAsync(const QStringList &screensId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["screen_id_array"] = toJsonArray(screensId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/screen/remove", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/tunnel/start/" + tunnelType, profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTunnelStopAsync(const QString &tunnelId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/tunnel/stop", profile.GetAccessToken(), jsonData, callback);
}

auto HttpReqTunnelSetInfoAsync(const QString &tunnelId, const QString &info, AuthProfile &profile,
                               const HttpCallback &callback) -> void {
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    dataJson["p_info"] = info;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/tunnel/set/info", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqChatSendMessageAsync(const QString &text, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["message"] = text;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/chat/send", profile.GetAccessToken(), jsonData, callback);
}

void HttpReqServiceCallAsync(const QString &service, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["service"] = service;
    dataJson["command"] = command;
    dataJson["args"] = args;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/service/call", profile.GetAccessToken(), jsonData, callback);
}
