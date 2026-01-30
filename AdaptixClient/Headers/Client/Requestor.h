#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/HttpRequestManager.h>

class AuthProfile;

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, int timeout = 8000 );

/// CLIENT

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok);

/// ASYNC VERSIONS

void HttpReqAgentRemoveAsync(const QStringList &agentsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetTagAsync(const QStringList &agentsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetMarkAsync(const QStringList &agentsId, const QString &mark, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentSetColorAsync(const QStringList &agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentUpdateDataAsync(const QString &agentId, const QJsonObject &updateData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentGenerateAsync(const QString &listenerName, const QString &agentName, const QString &configData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile);
void HttpReqAgentCommandFileAsync(const QByteArray &jsonData, AuthProfile& profile);
void HttpReqConsoleRemoveAsync(const QStringList &agentsId, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTaskCancelAsync(const QString &agentId, const QStringList &tasksId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksDeleteAsync(const QString &agentId, const QStringList &tasksId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTasksSaveAsync(const QString &agentId, const QString &CommandLine, int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, const HttpCallback &callback);

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsRemoveAsync(const QStringList &credsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqCredentialsSetTagAsync(const QStringList &credsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetRemoveAsync(const QStringList &targetsId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTargetSetTagAsync(const QStringList &targetsId, const QString &tag, AuthProfile& profile, const HttpCallback &callback);

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback);
void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, const HttpCallback &callback);

void HttpReqDownloadActionAsync(const QString &action, const QString &fileId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqDownloadDelete(const QStringList &fileId, AuthProfile& profile, const HttpCallback &callback);

void HttpReqScreenSetNoteAsync(const QStringList &screensId, const QString &note, AuthProfile& profile, const HttpCallback &callback);
void HttpReqScreenRemoveAsync(const QStringList &screensId, AuthProfile& profile, const HttpCallback &callback);

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelStopAsync(const QString &tunnelId, AuthProfile& profile, const HttpCallback &callback);
void HttpReqTunnelSetInfoAsync(const QString &tunnelId, const QString &info, AuthProfile& profile, const HttpCallback &callback);

void HttpReqChatSendMessageAsync(const QString &text, AuthProfile& profile, const HttpCallback &callback);

void HttpReqServiceCallAsync(const QString &service, const QString &command, const QString &args, AuthProfile& profile, const HttpCallback &callback);

#endif
