#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/HttpRequestManager.h>
#include <QUrlQuery>

static inline void httpPost(AuthProfile& profile, const QString& endpoint, const QJsonObject& body, const HttpCallback& callback, int timeout = 8000){
    HttpRequestManager::instance().post(profile.GetURL(), endpoint, profile.GetAccessToken(), QJsonDocument(body).toJson(), callback, timeout);
}

static inline void httpPostRaw(AuthProfile& profile, const QString& endpoint, const QByteArray& jsonData, const HttpCallback& callback, int timeout = 8000){
    HttpRequestManager::instance().post(profile.GetURL(), endpoint, profile.GetAccessToken(), jsonData, callback, timeout);
}

static inline void httpPostFF(AuthProfile& profile, const QString& endpoint, const QByteArray& jsonData){
    HttpRequestManager::instance().postFireAndForget(profile.GetURL(), endpoint, profile.GetAccessToken(), jsonData);
}

static inline void httpGet(AuthProfile& profile, const QString& endpoint, const QUrlQuery& params, const HttpCallback& callback, int timeout = 8000) {
    HttpRequestManager::instance().getPage(profile.GetURL(), endpoint, profile.GetAccessToken(), params, callback, timeout);
}

static QJsonArray i64Array(const QList<qint64>& ids)
{
    QJsonArray arr;
    for (qint64 id : ids)
        arr.append(toJsonI64(id));
    return arr;
}

static void postIdArray(AuthProfile& profile, const QString& endpoint, const char* key, const QList<qint64>& ids, const HttpCallback& callback)
{
    QJsonObject jsonData;
    jsonData[key] = i64Array(ids);
    httpPost(profile, endpoint, jsonData, callback);
}

static void postIdArrayTag(AuthProfile& profile, const QString& endpoint, const char* key, const QList<qint64>& ids, const QString& tag, const HttpCallback& callback)
{
    QJsonObject jsonData;
    jsonData[key]   = i64Array(ids);
    jsonData["tag"] = tag;
    httpPost(profile, endpoint, jsonData, callback);
}

static void postListenerNameType(AuthProfile& profile, const QString& endpoint, const QString& name, const QString& type, const HttpCallback& callback)
{
    QJsonObject jsonData;
    jsonData["name"] = name;
    jsonData["type"] = type;
    httpPost(profile, endpoint, jsonData, callback);
}


QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, const int timeout)
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QNetworkRequest request{QUrl(sUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setSslConfiguration(sslConfig);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

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
        QJsonParseError parseError;
        const QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject())
            jsonObject = jsonResponse.object();
    }
    reply->deleteLater();
    return jsonObject;
}

bool HttpReqLogin(AuthProfile* profile)
{
    QJsonObject dataJson;
    dataJson["username"] = profile->GetUsername();
    dataJson["password"] = profile->GetPassword();

    const QJsonObject jsonObject = HttpReq(profile->GetURL() + "/login", QJsonDocument(dataJson).toJson(), QString());
    if (!jsonObject.contains("access_token") || !jsonObject.contains("refresh_token") || !jsonObject.contains("version"))
        return false;

    const QString version = jsonObject["version"].toString();
    if (version != SMALL_VERSION) {
        profile->message = QString("Version mismatch: Server %1, Client %2").arg(version, SMALL_VERSION);
        return false;
    }
    profile->SetAccessToken(jsonObject["access_token"].toString());
    profile->SetRefreshToken(jsonObject["refresh_token"].toString());
    return true;
}

bool HttpReqJwtUpdate(AuthProfile* profile)
{
    const QJsonObject jsonObject = HttpReq(profile->GetURL() + "/refresh", QJsonDocument(QJsonObject()).toJson(), profile->GetRefreshToken());
    if (!jsonObject.contains("access_token"))
        return false;
    profile->SetAccessToken(jsonObject["access_token"].toString());
    return true;
}

bool HttpReqGetOTP(const QString &type, const QJsonObject &data, AuthProfile& profile, QString* message, bool* ok)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = data;
    const QJsonObject jsonObject = HttpReq(profile.GetURL() + "/otp/generate", QJsonDocument(dataJson).toJson(), profile.GetAccessToken());
    if (!jsonObject.contains("message") || !jsonObject.contains("ok"))
        return false;
    *message = jsonObject["message"].toString();
    *ok = jsonObject["ok"].toBool();
    return true;
}

void HttpReqGetOTPAsync(const QString &type, const QJsonObject &data, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["type"] = type;
    dataJson["data"] = data;
    httpPost(profile, "/otp/generate", dataJson, callback);
}

void HttpReqAgentRemoveAsync(const QList<qint64> &agentsId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/agent/remove", "agent_id_array", agentsId, callback);
}

void HttpReqAgentSetTagAsync(const QList<qint64> &agentsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback) {
    postIdArrayTag(profile, "/agent/set/tag", "agent_id_array", agentsId, tag, callback);
}

void HttpReqAgentSetMarkAsync(const QList<qint64> &agentsId, const QString &mark, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = i64Array(agentsId);
    dataJson["mark"] = mark;
    httpPost(profile, "/agent/set/mark", dataJson, callback);
}

void HttpReqAgentSetColorAsync(const QList<qint64> &agentsId, const QString &background, const QString &foreground, const bool reset, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id_array"] = i64Array(agentsId);
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

void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    httpPostFF(profile, "/agent/command/execute", jsonData);
}

void HttpReqAgentCommandFileAsync(const QByteArray &jsonData, AuthProfile& profile)
{
    httpPostFF(profile, "/agent/command/file", jsonData);
}

void HttpReqConsoleGetPageAsync(qint64 agentId, qint64 afterId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    if (afterId > 0)
        params.addQueryItem("after_id", QString::number(afterId));
    params.addQueryItem("limit", QString::number(limit));
    httpGet(profile, "/agent/console/list", params, callback);
}

void HttpReqConsoleGetAroundAsync(qint64 agentId, qint64 aroundId, int limit, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    params.addQueryItem("around_id", QString::number(aroundId));
    params.addQueryItem("limit", QString::number(limit));
    httpGet(profile, "/agent/console/list", params, callback);
}

void HttpReqConsoleSearchAsync(qint64 agentId, const QString &query, int limit, int offset, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("agent_id", QString::number(agentId));
    params.addQueryItem("q", query);
    params.addQueryItem("limit", QString::number(limit));
    if (offset > 0)
        params.addQueryItem("offset", QString::number(offset));
    httpGet(profile, "/agent/console/search", params, callback, 60000);
}

void HttpReqLogsGetPageAsync(int offset, int limit, qint64 beforeId, const QString &source, const QString &category, const QString &contains, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    if (beforeId > 0)
        params.addQueryItem("before_id", QString::number(beforeId));
    else
        params.addQueryItem("offset", QString::number(offset));
    params.addQueryItem("limit", QString::number(limit));
    if (!source.isEmpty())
        params.addQueryItem("source", source);
    if (!category.isEmpty())
        params.addQueryItem("category", category);
    if (!contains.isEmpty())
        params.addQueryItem("q", contains);
    httpGet(profile, "/logs/list", params, callback);
}

void HttpReqTaskCancelAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = i64Array(tasksId);
    httpPost(profile, "/agent/task/cancel", dataJson, callback);
}

void HttpReqTasksDeleteAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = agentId;
    dataJson["tasks_array"] = i64Array(tasksId);
    httpPost(profile, "/agent/task/delete", dataJson, callback);
}

void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback) {
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

void HttpReqCredentialsRemoveAsync(const QList<qint64> &credsId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/creds/remove", "cred_id_array", credsId, callback);
}

void HttpReqCredentialsSetTagAsync(const QList<qint64> &credsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback) {
    postIdArrayTag(profile, "/creds/set/tag", "id_array", credsId, tag, callback);
}

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback) {
    httpPostRaw(profile, "/targets/add", jsonData, callback);
}

void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback) {
    httpPostRaw(profile, "/targets/edit", jsonData, callback);
}

void HttpReqTargetRemoveAsync(const QList<qint64> &targetsId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/targets/remove", "target_id_array", targetsId, callback);
}

void HttpReqTargetSetTagAsync(const QList<qint64> &targetsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback) {
    postIdArrayTag(profile, "/targets/set/tag", "id_array", targetsId, tag, callback);
}

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, const QString &tags, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    dataJson["tags"]   = tags;
    httpPost(profile, "/listener/create", dataJson, callback);
}

void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, const QString &tags, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"]   = listenerName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    dataJson["tags"]   = tags;
    httpPost(profile, "/listener/edit", dataJson, callback);
}

void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback) {
    postListenerNameType(profile, "/listener/stop", listenerName, listenerType, callback);
}

void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback) {
    postListenerNameType(profile, "/listener/pause", listenerName, listenerType, callback);
}

void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback) {
    postListenerNameType(profile, "/listener/resume", listenerName, listenerType, callback);
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

void HttpReqDownloadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/download/delete", "file_id_array", fileId, callback);
}

void HttpReqUploadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/upload/delete", "id_array", fileId, callback);
}

void HttpReqDownloadSetTag(const QList<qint64> &fileId, const QString &tag, AuthProfile& profile, const HttpCallback &callback) {
    postIdArrayTag(profile, "/download/set/tag", "id_array", fileId, tag, callback);
}

void HttpReqScreenSetNoteAsync(const QList<qint64> &screensId, const QString &note, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["screen_id_array"] = i64Array(screensId);
    dataJson["note"] = note;
    httpPost(profile, "/screen/setnote", dataJson, callback);
}

void HttpReqScreenRemoveAsync(const QList<qint64> &screensId, AuthProfile& profile, const HttpCallback &callback) {
    postIdArray(profile, "/screen/remove", "screen_id_array", screensId, callback);
}

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback) {
    HttpRequestManager::instance().post(profile.GetURL(), "/tunnel/start/" + tunnelType, profile.GetAccessToken(), jsonData, callback);
}

void HttpReqTunnelStopAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    httpPost(profile, "/tunnel/stop", dataJson, callback);
}

void HttpReqTunnelPauseAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    httpPost(profile, "/tunnel/pause", dataJson, callback);
}

void HttpReqTunnelResumeAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["p_tunnel_id"] = tunnelId;
    httpPost(profile, "/tunnel/resume", dataJson, callback);
}

void HttpReqTunnelSetInfoAsync(qint64 tunnelId, const QString &info, AuthProfile &profile, const HttpCallback &callback)
{
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

void HttpReqChatEditMessageAsync(qint64 id, const QString &text, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["message"] = text;
    httpPost(profile, "/chat/" + QString::number(id) + "/edit", dataJson, callback);
}

void HttpReqChatDeleteMessageAsync(qint64 id, AuthProfile& profile, const HttpCallback &callback) {
    httpPost(profile, "/chat/" + QString::number(id) + "/delete", QJsonObject(), callback);
}

void HttpReqChatReactionAsync(qint64 id, const QString &emoji, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["emoji"] = emoji;
    httpPost(profile, "/chat/" + QString::number(id) + "/react", dataJson, callback);
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

void HttpReqChatClearAsync(AuthProfile& profile, const HttpCallback &callback) {
    httpPost(profile, "/chat/clear", QJsonObject(), callback);
}

void HttpReqPluginServiceCallAsync(const QString &service, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["service"] = service;
    dataJson["command"] = command;
    dataJson["args"] = args;
    httpPost(profile, "/plugin/service/call", dataJson, callback);
}

void HttpReqPluginAgentCallAsync(qint64 agentId, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["agent_id"] = toJsonI64(agentId);
    dataJson["command"] = command;
    dataJson["args"] = args;
    httpPost(profile, "/plugin/agent/call", dataJson, callback);
}

void HttpReqPluginListenerCallAsync(const QString &listenerName, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["listener"] = listenerName;
    dataJson["command"] = command;
    dataJson["args"] = args;
    httpPost(profile, "/plugin/listener/call", dataJson, callback);
}

void HttpReqEventHandlerRegisterAsync(const QJsonObject &body, AuthProfile& profile, const HttpCallback &callback)
{
    httpPost(profile, "/events/handlers/register", body, callback);
}

void HttpReqEventHandlerGetAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id"] = id;
    httpPost(profile, "/events/handlers/get", dataJson, callback);
}

void HttpReqEventHandlerEnableAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id"] = id;
    httpPost(profile, "/events/handlers/enable", dataJson, callback);
}

void HttpReqEventHandlerDisableAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id"] = id;
    httpPost(profile, "/events/handlers/disable", dataJson, callback);
}

void HttpReqEventHandlerRemoveAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id"] = id;
    httpPost(profile, "/events/handlers/remove", dataJson, callback);
}

void HttpReqEventMuteAsync(const QString &eventType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["event"] = eventType;
    httpPost(profile, "/events/mute", dataJson, callback);
}

void HttpReqEventUnmuteAsync(const QString &eventType, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["event"] = eventType;
    httpPost(profile, "/events/unmute", dataJson, callback);
}

void HttpReqEventMutesListAsync(AuthProfile& profile, const HttpCallback &callback)
{
    httpPost(profile, "/events/mutes", QJsonObject{}, callback);
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

void HttpReqGroupMoveMemberAsync(qint64 agentId, int64_t fromGroupId, int64_t toGroupId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject body;
    body["agent_id"] = toJsonI64(agentId);
    body["from_group_id"] = toJsonI64(fromGroupId);
    body["to_group_id"] = toJsonI64(toGroupId);
    httpPost(profile, "/group/move_member", body, callback);
}

void HttpReqGroupReparentAsync(int64_t groupId, int64_t newParentId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["group_id"]      = toJsonI64(groupId);
    dataJson["new_parent_id"] = toJsonI64(newParentId);
    httpPost(profile, "/group/reparent", dataJson, callback);
}


void HttpReqPayloadListAsync(bool showHidden, AuthProfile& profile, const HttpCallback &callback)
{
    QUrlQuery params;
    params.addQueryItem("show_hidden", showHidden ? "1" : "0");
    httpGet(profile, "/payload/list", params, callback);
}

void HttpReqPayloadSyncAsync(AuthProfile& profile, const HttpCallback &callback)
{
    httpGet(profile, "/payload/sync", QUrlQuery(), callback);
}

void HttpReqPayloadGetAsync(qint64 payloadId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["payload_id"] = toJsonI64(payloadId);
    httpPost(profile, "/payload/get", dataJson, callback);
}

void HttpReqPayloadDownloadAsync(qint64 payloadId, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["payload_id"] = toJsonI64(payloadId);
    httpPost(profile, "/payload/download", dataJson, callback, 60000);
}

void HttpReqPayloadHideAsync(const QList<qint64> &ids, bool hidden, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id_array"] = i64Array(ids);
    dataJson["hidden"] = hidden;
    httpPost(profile, "/payload/hide", dataJson, callback);
}

void HttpReqPayloadSetColorAsync(const QList<qint64> &ids, const QString &background, const QString &foreground, const bool reset, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id_array"] = i64Array(ids);
    dataJson["background"] = background;
    dataJson["foreground"] = foreground;
    dataJson["reset"] = reset;
    httpPost(profile, "/payload/set_color", dataJson, callback);
}

void HttpReqPayloadUpdateAsync(qint64 payloadId, const QString &name, const QString &notes, const QString &artifact, const QString &arch, bool hidden, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["payload_id"] = toJsonI64(payloadId);
    dataJson["name"] = name;
    dataJson["notes"] = notes;
    dataJson["artifact"] = artifact;
    dataJson["arch"] = arch;
    dataJson["hidden"] = hidden;
    httpPost(profile, "/payload/update", dataJson, callback);
}

void HttpReqPayloadRemoveAsync(const QList<qint64> &ids, bool hard, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["id_array"] = i64Array(ids);
    dataJson["hard"] = hard;
    httpPost(profile, "/payload/remove", dataJson, callback);
}

void HttpReqPayloadImportAsync(const QString &name, const QString &agentType, const QString &artifact, const QString &arch, const QStringList &listeners, const QByteArray &content, const QString &config, AuthProfile& profile, const HttpCallback &callback)
{
    QJsonObject dataJson;
    dataJson["name"] = name;
    dataJson["agent_type"] = agentType;
    dataJson["artifact"] = artifact;
    dataJson["arch"] = arch;
    dataJson["listeners"] = toJsonArray(listeners);
    dataJson["content"] = QString::fromLatin1(content.toBase64());
    dataJson["config"] = config;
    httpPost(profile, "/payload/import", dataJson, callback, 60000);
}
