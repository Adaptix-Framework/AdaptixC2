#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/AuthProfile.h>

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token );

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

/// LISTENER

bool HttpReqListenerStart(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerEdit(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerStop(const QString &listenerName, const QString &listenerType, AuthProfile profile, QString* message, bool* ok );

/// AGENT

bool HttpReqAgentGenerate(const QString &listenerName, const QString &listenerType, const QString &agentName, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentCommand(const QString &agentName, const QString &agentId, const QString &cmdLine, const QString &data, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentExit( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetTag( QStringList agentsId, const QString &tag, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetMark( QStringList agentsId, const QString &mark, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetColor( QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTaskStop(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTasksDelete(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

///DOWNLOAD

bool HttpReqBrowserDownload(const QString &action, const QString &fileId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserDownloadStart(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok );

///BROWSER

bool HttpReqBrowserDisks(const QString &agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserProcess(const QString &agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserList(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserUpload(const QString &agentId, const QString &path, const QString &content, AuthProfile profile, QString* message, bool* ok );

///TUNNEL

bool HttpReqTunnelStop(const QString &tunnelId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTunnelSetInfo(const QString &tunnelId, const QString &info, AuthProfile profile, QString* message, bool* ok );

#endif //ADAPTIXCLIENT_REQUESTOR_H
