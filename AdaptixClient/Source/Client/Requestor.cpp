#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/HttpRequestManager.h>
#include <QUrlQuery>

static inline void httpPost(AuthProfile& profile, const QString& endpoint, const QJsonObject& body, const HttpCallback& callback, int timeout = 8000) {
    QByteArray data = QJsonDocument(body).toJson();
    HttpRequestManager::instance().post(profile.GetURL(), endpoint, profile.GetAccessToken(), data, callback, timeout);
}

static inline void httpPostRaw(AuthProfile& profile, const QString& endpoint, const QByteArray& jsonData, const HttpCallback& callback, int timeout = 8000) {
    HttpRequestManager::instance().post(profile.GetURL(), endpoint, profile.GetAccessToken(), jsonData, callback, timeout);
}

static inline void httpPostFF(AuthProfile& profile, const QString& endpoint, const QByteArray& jsonData) {
    HttpRequestManager::instance().postFireAndForget(profile.GetURL(), endpoint, profile.GetAccessToken(), jsonData);
}



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
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
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
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString sUrl = profile->GetURL() + "/login";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, QString());
    if (jsonObject.contains("access_token") && jsonObject.contains("refresh_token") && jsonObject.contains("version")) {
        QString version = jsonObject["version"].toString();
        if ( version != SMALL_VERSION) {
            profile->message = QString("Version mismatch: Server %1, Client %2").arg(version).arg(SMALL_VERSION);
            return false;
        }
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

bool HttpReqGetOTP(const QString &type, const QJsonObject &data, AuthProfile& profile, QString* message, bool* ok)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = data;
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

void HttpReqGetOTPAsync(const QString &type, const QJsonObject &data, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = data;
    httpPost(profile, "/otp/generate", dataJson, callback);
}

/// ASYNC VERSIONS

static QJsonArray toJsonArrayInt64(const QList<qint64> &ids)
{
    QJsonArray arr;
    for (qint64 id : ids) arr.append(id);
    return arr;
}

void HttpReqAgentRemoveAsync(const QList<qint64> &agentsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArrayInt64(agentsId);
    httpPost(profile, "/agent/remove", dataJson, callback);
}

void HttpReqAgentSetTagAsync(const QList<qint64> &agentsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArrayInt64(agentsId);
    dataJson["tag"] = tag;
    httpPost(profile, "/agent/set/tag", dataJson, callback);
}

void HttpReqAgentSetMarkAsync(const QList<qint64> &agentsId, const QString &mark, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArrayInt64(agentsId);
    dataJson["mark"] = mark;
    httpPost(profile, "/agent/set/mark", dataJson, callback);
}

void HttpReqAgentSetColorAsync(const QList<qint64> &agentsId, const QString &background, const QString &foreground, const bool reset, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArrayInt64(agentsId);
    dataJson["bc"] = background;
    dataJson["fc"] = foreground;
    dataJson["reset"] = reset;
    httpPost(profile, "/agent/set/color", dataJson, callback);
}

void HttpReqAgentUpdateDataAsync(qint64 agentId, const QJsonObject &updateData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson = updateData;
    dataJson["agent_id"] = agentId;
    httpPost(profile, "/agent/update/data", dataJson, callback);
}

void HttpReqAgentGenerateAsync(const QString &listenerName, const QString &agentName, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["listener_name"] = listenerName;
    dataJson["agent"]         = agentName;
    dataJson["config"]        = configData;
    httpPost(profile, "/agent/generate", dataJson, callback, 30000);
}

void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    httpPostFF(profile, "/agent/command/execute", jsonData);
}

void HttpReqAgentCommandFileAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    httpPostFF(profile, "/agent/command/file", jsonData);
}



void HttpReqConsoleRemoveAsync(const QList<qint64> &agentsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = toJsonArrayInt64(agentsId);
    httpPost(profile, "/agent/console/remove", dataJson, callback);
}

void HttpReqConsoleGetPageAsync(qint64 agentId, qint64 afterId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    if (afterId > 0)
        params.addQueryItem("after_id", QString::number(afterId));
    params.addQueryItem("limit",    QString::number(limit));

    HttpRequestManager::instance().getPage(profile.GetURL(), "/agent/console/list", profile.GetAccessToken(), params, callback);
}

void HttpReqConsoleGetAroundAsync(qint64 agentId, qint64 aroundId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    params.addQueryItem("around_id", QString::number(aroundId));
    params.addQueryItem("limit", QString::number(limit));

    HttpRequestManager::instance().getPage(profile.GetURL(), "/agent/console/list", profile.GetAccessToken(), params, callback);
}

void HttpReqConsoleSearchAsync(qint64 agentId, const QString &query, int limit, int offset, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    params.addQueryItem("q", query);
    params.addQueryItem("limit", QString::number(limit));
    if (offset > 0)
        params.addQueryItem("offset", QString::number(offset));

    HttpRequestManager::instance().getPage(profile.GetURL(), "/agent/console/search", profile.GetAccessToken(), params, callback, 60000);
}

void HttpReqLogsGetPageAsync(int offset, int limit, qint64 beforeId, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    if (beforeId > 0) {
        params.addQueryItem("before_id", QString::number(beforeId));
    } else {
        params.addQueryItem("offset", QString::number(offset));
    }
    params.addQueryItem("limit", QString::number(limit));

    HttpRequestManager::instance().getPage(profile.GetURL(), "/logs/list", profile.GetAccessToken(), params, callback);
}

void HttpReqTaskCancelAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = toJsonArray(tasksId);
    httpPost(profile, "/agent/task/cancel", dataJson, callback);
}

void HttpReqTasksDeleteAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = toJsonArray(tasksId);
    httpPost(profile, "/agent/task/delete", dataJson, callback);
}

void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    httpPostRaw(profile, "/agent/task/hook", jsonData, callback);
}

void HttpReqTasksSaveAsync(qint64 agentId, const QString &CommandLine, const int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"]     = agentId;
    dataJson["command_line"] = CommandLine;
    dataJson["message_type"] = MessageType;
    dataJson["message"]      = Message;
    dataJson["clear_text"]   = ClearText;
    httpPost(profile, "/agent/task/save", dataJson, callback);
}

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    httpPostRaw(profile, "/creds/add", jsonData, callback);
}

void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    httpPostRaw(profile, "/creds/edit", jsonData, callback);
}

void HttpReqCredentialsRemoveAsync(const QList<qint64> &credsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : credsId) arr.append(id);
    dataJson["cred_id_array"] = arr;
    httpPost(profile, "/creds/remove", dataJson, callback);
}

void HttpReqCredentialsSetTagAsync(const QList<qint64> &credsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : credsId) arr.append(id);
    dataJson["id_array"] = arr;
    dataJson["tag"] = tag;
    httpPost(profile, "/creds/set/tag", dataJson, callback);
}

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    httpPostRaw(profile, "/targets/add", jsonData, callback);
}

void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    httpPostRaw(profile, "/targets/edit", jsonData, callback);
}

void HttpReqTargetRemoveAsync(const QList<qint64> &targetsId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : targetsId) arr.append(id);
    dataJson["target_id_array"] = arr;
    httpPost(profile, "/targets/remove", dataJson, callback);
}

void HttpReqTargetSetTagAsync(const QList<qint64> &targetsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : targetsId) arr.append(id);
    dataJson["id_array"] = arr;
    dataJson["tag"] = tag;
    httpPost(profile, "/targets/set/tag", dataJson, callback);
}

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    httpPost(profile, "/listener/create", dataJson, callback);
}

void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    httpPost(profile, "/listener/edit", dataJson, callback);
}

void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    httpPost(profile, "/listener/stop", dataJson, callback);
}

void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    httpPost(profile, "/listener/pause", dataJson, callback);
}

void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    httpPost(profile, "/listener/resume", dataJson, callback);
}

void HttpReqListenerSetTagsAsync(const QString &listenerName, const QString &tags, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["tags"] = tags;
    httpPost(profile, "/listener/tags", dataJson, callback);
}

void HttpReqListenerConnectorAsync(const QString &listenerName, const QString &data, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["listener_name"] = listenerName;
    dataJson["data"]          = data;
    httpPost(profile, "/listener/connector", dataJson, callback);
}

void HttpReqDownloadActionAsync(const QString &action, qint64 fileId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["file_id"] = toJsonI64(fileId);
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpRequestManager::instance().post(profile.GetURL(), "/download/" + action, profile.GetAccessToken(), jsonData, callback);
}

void HttpReqDownloadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : fileId)
        arr.append(toJsonI64(id));
    dataJson["file_id_array"] = arr;
    httpPost(profile, "/download/delete", dataJson, callback);
}

void HttpReqUploadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : fileId)
        arr.append(toJsonI64(id));
    dataJson["id_array"] = arr;
    httpPost(profile, "/upload/delete", dataJson, callback);
}

void HttpReqDownloadSetTag(const QList<qint64> &fileId, const QString &tag, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : fileId)
        arr.append(toJsonI64(id));
    dataJson["id_array"] = arr;
    dataJson["tag"] = tag;
    httpPost(profile, "/download/set/tag", dataJson, callback);
}

void HttpReqScreenSetNoteAsync(const QList<qint64> &screensId, const QString &note, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : screensId)
        arr.append(id);
    dataJson["screen_id_array"] = arr;
    dataJson["note"] = note;
    httpPost(profile, "/screen/setnote", dataJson, callback);
}

void HttpReqScreenRemoveAsync(const QList<qint64> &screensId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    QJsonArray arr;
    for (qint64 id : screensId)
        arr.append(id);
    dataJson["screen_id_array"] = arr;
    httpPost(profile, "/screen/remove", dataJson, callback);
}

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback)
{
    HttpRequestManager::instance().post(profile.GetURL(), "/tunnel/start/" + tunnelType, profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTunnelStopAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    httpPost(profile, "/tunnel/stop", dataJson, callback);
}

auto HttpReqTunnelSetInfoAsync(qint64 tunnelId, const QString &info, AuthProfile &profile,
                               const HttpCallback &callback) -> void {
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    dataJson["p_info"] = info;
    httpPost(profile, "/tunnel/set/info", dataJson, callback);
}

void HttpReqChatSendMessageAsync(const QString &text, qint64 replyToId, const QString &replyToName, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["message"] = text;
    dataJson["reply_to_id"] = static_cast<double>(replyToId);
    dataJson["reply_to_name"] = replyToName;
    httpPost(profile, "/chat/send", dataJson, callback);
}

static inline void httpGet(AuthProfile& profile, const QString& endpoint, const QUrlQuery& params, const HttpCallback& callback, int timeout = 8000) {
    HttpRequestManager::instance().getPage(profile.GetURL(), endpoint, profile.GetAccessToken(), params, callback, timeout);
}

void HttpReqChatEditMessageAsync(qint64 id, const QString &text, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["message"] = text;
    httpPost(profile, "/chat/" + QString::number(id) + "/edit", dataJson, callback);
}

void HttpReqChatDeleteMessageAsync(qint64 id, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    httpPost(profile, "/chat/" + QString::number(id) + "/delete", dataJson, callback);
}

void HttpReqChatReactionAsync(qint64 id, const QString &emoji, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["emoji"] = emoji;
    httpPost(profile, "/chat/" + QString::number(id) + "/react", dataJson, callback);
}

void HttpReqChatGetTodoAsync(AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    httpGet(profile, "/chat/todo", params, callback);
}

void HttpReqChatUpdateTodoAsync(const QString &content, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["content"] = content;
    httpPost(profile, "/chat/todo", dataJson, callback);
}

void HttpReqChatHistoryAsync(qint64 beforeId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    if (beforeId > 0)
        params.addQueryItem("before_id", QString::number(beforeId));
    params.addQueryItem("limit", QString::number(limit));
    httpGet(profile, "/chat/history", params, callback);
}

void HttpReqChatSearchAsync(const QString &query, qint64 beforeId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("q", query);
    if (beforeId > 0)
        params.addQueryItem("before_id", QString::number(beforeId));
    params.addQueryItem("limit", QString::number(limit));
    httpGet(profile, "/chat/search", params, callback);
}

void HttpReqChatClearAsync(AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    httpPost(profile, "/chat/clear", dataJson, callback);
}

void HttpReqServiceCallAsync(const QString &service, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["service"] = service;
    dataJson["command"] = command;
    dataJson["args"] = args;
    httpPost(profile, "/service/call", dataJson, callback);
}

void HttpReqAxScriptListAsync(AuthProfile& profile, const HttpCallback &callback)
{
    QByteArray jsonData = QJsonDocument(QJsonObject()).toJson();
    httpPostRaw(profile, "/axscript/list", jsonData, callback);
}

void HttpReqAxScriptCommandsAsync(AuthProfile& profile, const HttpCallback &callback)
{
    QByteArray jsonData = QJsonDocument(QJsonObject()).toJson();
    httpPostRaw(profile, "/axscript/commands", jsonData, callback);
}

void HttpReqAxScriptLoadAsync(const QString &name, const QString &script, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = name;
    dataJson["script"] = script;
    httpPost(profile, "/axscript/load", dataJson, callback);
}

void HttpReqAxScriptUnloadAsync(const QString &name, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = name;
    httpPost(profile, "/axscript/unload", dataJson, callback);
}

void HttpReqGroupListAsync(const QString &scope, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("scope", scope);
    HttpRequestManager::instance().getPage(profile.GetURL(), "/group/list", profile.GetAccessToken(), params, callback);
}

void HttpReqGroupCreateAsync(int64_t parentId, const QString &name, const QString &scope, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["parent_id"] = toJsonI64(parentId);
    dataJson["name"]      = name;
    dataJson["scope"]     = scope;
    httpPost(profile, "/group/create", dataJson, callback);
}

void HttpReqGroupRenameAsync(int64_t groupId, const QString &name, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["group_id"] = toJsonI64(groupId);
    dataJson["name"]     = name;
    httpPost(profile, "/group/rename", dataJson, callback);
}

void HttpReqGroupDeleteAsync(int64_t groupId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["group_id"] = toJsonI64(groupId);
    httpPost(profile, "/group/delete", dataJson, callback);
}

void HttpReqGroupMembersAsync(int64_t groupId, const QList<qint64> &add, const QList<qint64> &remove, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["group_id"] = toJsonI64(groupId);
    QJsonArray addArr, removeArr;
    for (qint64 id : add)
        addArr.append(toJsonI64(id));
    for (qint64 id : remove)
        removeArr.append(toJsonI64(id));
    dataJson["add"]    = addArr;
    dataJson["remove"] = removeArr;
    httpPost(profile, "/group/members", dataJson, callback);
}


void HttpReqGroupReparentAsync(int64_t groupId, int64_t newParentId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["group_id"]      = toJsonI64(groupId);
    dataJson["new_parent_id"] = toJsonI64(newParentId);
    httpPost(profile, "/group/reparent", dataJson, callback);
}

