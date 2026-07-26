#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/HttpRequestManager.h>

class AuthProfile;

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, int timeout = 8000);

/// CLIENT

bool HttpReqLogin(AuthProfile* profile);
bool HttpReqJwtUpdate(AuthProfile* profile);
bool HttpReqGetOTP(const QString &type, const QJsonObject &data, AuthProfile& profile, QString* message, bool* ok);

void HttpReqGetOTPAsync(const QString &type, const QJsonObject &data, AuthProfile& profile, const HttpCallback &callback);

/// ASYNC VERSIONS

void HttpReqAgentRemoveAsync(const QList<qint64> &agentsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetTagAsync(const QList<qint64> &agentsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetMarkAsync(const QList<qint64> &agentsId, const QString &mark, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetColorAsync(const QList<qint64> &agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentUpdateDataAsync(qint64 agentId, const QJsonObject &updateData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile);
void HttpReqAgentCommandFileAsync(const QByteArray &jsonData, AuthProfile& profile);

void HttpReqConsoleRemoveAsync(const QList<qint64> &agentsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqConsoleGetPageAsync(qint64 agentId, qint64 afterId, int limit, AuthProfile& profile, const HttpCallback &callback);
void HttpReqConsoleGetAroundAsync(qint64 agentId, qint64 aroundId, int limit, AuthProfile& profile, const HttpCallback &callback);
void HttpReqConsoleSearchAsync(qint64 agentId, const QString &query, int limit, int offset, AuthProfile& profile, const HttpCallback &callback);

void HttpReqLogsGetPageAsync(int offset, int limit, qint64 beforeId, const QString &source, const QString &category, const QString &contains, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTaskCancelAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksDeleteAsync(qint64 agentId, const QList<qint64> &tasksId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksSaveAsync(qint64 agentId, const QString &CommandLine, int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, const HttpCallback &callback);

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsRemoveAsync(const QList<qint64> &credsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsSetTagAsync(const QList<qint64> &credsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetRemoveAsync(const QList<qint64> &targetsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetSetTagAsync(const QList<qint64> &targetsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, const QString &tags, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, const QString &tags, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerConnectorAsync(const QString &listenerName, const QString &data, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerSetTagsAsync(const QString &listenerName, const QString &tags, AuthProfile& profile, const HttpCallback &callback);

void HttpReqDownloadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqUploadDelete(const QList<qint64> &fileId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqDownloadSetTag(const QList<qint64> &fileId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);

void HttpReqScreenSetNoteAsync(const QList<qint64> &screensId, const QString &note, AuthProfile& profile, const HttpCallback &callback);
void HttpReqScreenRemoveAsync(const QList<qint64> &screensId, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelStopAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelPauseAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelResumeAsync(qint64 tunnelId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelSetInfoAsync(qint64 tunnelId, const QString &info, AuthProfile& profile, const HttpCallback &callback);

void HttpReqChatSendMessageAsync(const QString &text, qint64 replyToId, const QString &replyToName, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatEditMessageAsync(qint64 id, const QString &text, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatDeleteMessageAsync(qint64 id, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatReactionAsync(qint64 id, const QString &emoji, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatUpdateTodoAsync(const QString &content, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatHistoryAsync(qint64 beforeId, int limit, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatSearchAsync(const QString &query, qint64 beforeId, int limit, AuthProfile& profile, const HttpCallback &callback);
void HttpReqChatClearAsync(AuthProfile& profile, const HttpCallback &callback);

void HttpReqServiceCallAsync(const QString &service, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback);

void HttpReqEventHandlerRegisterAsync(const QJsonObject &body, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventHandlerGetAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventHandlerEnableAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventHandlerDisableAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventHandlerRemoveAsync(const QString &id, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventMuteAsync(const QString &eventType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventUnmuteAsync(const QString &eventType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqEventMutesListAsync(AuthProfile& profile, const HttpCallback &callback);

void HttpReqGroupCreateAsync(int64_t parentId, const QString &name, const QString &scope, AuthProfile& profile, const HttpCallback &callback);
void HttpReqGroupRenameAsync(int64_t groupId, const QString &name, AuthProfile& profile, const HttpCallback &callback);
void HttpReqGroupDeleteAsync(int64_t groupId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqGroupMoveMemberAsync(qint64 agentId, int64_t fromGroupId, int64_t toGroupId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqGroupReparentAsync(int64_t groupId, int64_t newParentId, AuthProfile& profile, const HttpCallback &callback);

#endif
