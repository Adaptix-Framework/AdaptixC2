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

void HttpReqAgentRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetTagAsync(QStringList agentsId, const QString &tag, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetMarkAsync(QStringList agentsId, const QString &mark, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetColorAsync(QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentUpdateDataAsync(const QString &agentId, const QJsonObject &updateData, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentGenerateAsync(const QString &listenerName, const QString &agentName, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqConsoleRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);

void HttpReqTaskCancelAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksDeleteAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksSaveAsync(const QString &agentId, const QString &CommandLine, int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, HttpCallback callback);

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsRemoveAsync(const QStringList &credsId, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsSetTagAsync(QStringList credsId, const QString &tag, AuthProfile& profile, HttpCallback callback);

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetRemoveAsync(const QStringList &targetsId, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetSetTagAsync(QStringList targetsId, const QString &tag, AuthProfile& profile, HttpCallback callback);

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerPauseAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerResumeAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback);

void HttpReqDownloadActionAsync(const QString &action, const QString &fileId, AuthProfile& profile, HttpCallback callback);
void HttpReqDownloadDelete(const QStringList &fileId, AuthProfile& profile, HttpCallback callback);

void HttpReqScreenSetNoteAsync(const QStringList &screensId, const QString &note, AuthProfile& profile, HttpCallback callback);
void HttpReqScreenRemoveAsync(const QStringList &screensId, AuthProfile& profile, HttpCallback callback);

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelStopAsync(const QString &tunnelId, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelSetInfoAsync(const QString &tunnelId, const QString &info, AuthProfile& profile, HttpCallback callback);

void HttpReqChatSendMessageAsync(const QString &text, AuthProfile& profile, HttpCallback callback);

#endif
