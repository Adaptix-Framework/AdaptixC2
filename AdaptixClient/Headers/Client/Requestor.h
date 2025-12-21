#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/HttpRequestManager.h>

class AuthProfile;

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, int timeout = 8000 );

QJsonObject HttpReqTimeout( int timeout, const QString &sUrl, const QByteArray &jsonData, const QString &token );

///CLIENT

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqSync(AuthProfile profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok);

///LISTENER

bool HttpReqListenerStart(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerEdit(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerStop(const QString &listenerName, const QString &listenerType, AuthProfile profile, QString* message, bool* ok );

///AGENT

bool HttpReqAgentGenerate(const QString &listenerName, const QString &listenerType, const QString &agentName, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentCommand(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqConsoleRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetTag( QStringList agentsId, const QString &tag, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetMark( QStringList agentsId, const QString &mark, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetColor( QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetImpersonate(const QString &agentId, const QString &impersonate, bool elevated, AuthProfile profile, QString* message, bool* ok );

///TASK

bool HttpReqTaskCancel(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTasksDelete(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTasksHook(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTasksSave(const QString &agentId, const QString &CommandLine, int MessageType, const QString &Message, const QString &ClearText, AuthProfile profile, QString* message, bool* ok );

///CHAT

bool HttpReqChatSendMessage(const QString &text, AuthProfile profile, QString* message, bool* ok );

///DOWNLOAD

bool HttpReqDownloadAction(const QString &action, const QString &fileId, AuthProfile profile, QString* message, bool* ok );

///TUNNEL

bool HttpReqTunnelStartServer(const QString &tunnelType, const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTunnelStop(const QString &tunnelId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTunnelSetInfo(const QString &tunnelId, const QString &info, AuthProfile profile, QString* message, bool* ok );

///SCREEN

bool HttpReqScreenSetNote( QStringList scrensId, const QString &note, AuthProfile profile, QString* message, bool* ok );

bool HttpReqScreenRemove( QStringList scrensId, AuthProfile profile, QString* message, bool* ok );

///CREDS

bool HttpReqCredentialsCreate(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqCredentialsEdit(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqCredentialsRemove(const QStringList &credsId, AuthProfile profile, QString* message, bool* ok);

bool HttpReqCredentialsSetTag( QStringList credsId, const QString &tag, AuthProfile profile, QString* message, bool* ok );

///TARGETS

bool HttpReqTargetsCreate(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTargetEdit(const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTargetRemove(const QStringList &targetsId, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTargetSetTag( QStringList targetsId, const QString &tag, AuthProfile profile, QString* message, bool* ok );

/// ASYNC VERSIONS

void HttpReqAgentRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetTagAsync(QStringList agentsId, const QString &tag, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetMarkAsync(QStringList agentsId, const QString &mark, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetColorAsync(QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetImpersonateAsync(const QString &agentId, const QString &impersonate, bool elevated, AuthProfile& profile, HttpCallback callback);
void HttpReqConsoleRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);

void HttpReqTaskCancelAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksDeleteAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);

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

void HttpReqDownloadActionAsync(const QString &action, const QString &fileId, AuthProfile& profile, HttpCallback callback);

void HttpReqScreenSetNoteAsync(QStringList screensId, const QString &note, AuthProfile& profile, HttpCallback callback);
void HttpReqScreenRemoveAsync(QStringList screensId, AuthProfile& profile, HttpCallback callback);

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelStopAsync(const QString &tunnelId, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelSetInfoAsync(const QString &tunnelId, const QString &info, AuthProfile& profile, HttpCallback callback);

void HttpReqChatSendMessageAsync(const QString &text, AuthProfile& profile, HttpCallback callback);

#endif
