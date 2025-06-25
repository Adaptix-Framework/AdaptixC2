#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>

class AuthProfile;

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, int timeout = 3000 );

QJsonObject HttpReqTimeout( int timeout, const QString &sUrl, const QByteArray &jsonData, const QString &token );

/// CLIENT

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqSync(AuthProfile profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok);

/// LISTENER

bool HttpReqListenerStart(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerEdit(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerStop(const QString &listenerName, const QString &listenerType, AuthProfile profile, QString* message, bool* ok );

/// AGENT

bool HttpReqAgentGenerate(const QString &listenerName, const QString &listenerType, const QString &agentName, const QString &os, const QString &configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentCommand(const QString &agentName, const QString &agentId, const QString &cmdLine, const QString &data, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentExit( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqConsoleRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetTag( QStringList agentsId, const QString &tag, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetMark( QStringList agentsId, const QString &mark, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetColor( QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTaskStop(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTasksDelete(const QString &agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

///DOWNLOAD

bool HttpReqDownloadStart(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok );

bool HttpReqDownloadAction(const QString &action, const QString &fileId, AuthProfile profile, QString* message, bool* ok );

///BROWSER

bool HttpReqBrowserDisks(const QString &agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserProcess(const QString &agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserList(const QString &agentId, const QString &path, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserUpload(const QString &agentId, const QString &path, const QString &content, AuthProfile profile, QString* message, bool* ok );

///TUNNEL

bool HttpReqTunnelStartServer(const QString &tunnelType, const QByteArray &jsonData, AuthProfile profile, QString* message, bool* ok);

bool HttpReqTunnelStop(const QString &tunnelId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTunnelSetInfo(const QString &tunnelId, const QString &info, AuthProfile profile, QString* message, bool* ok );

/// SCREEN

bool HttpReqScreenSetNote( QStringList scrensId, const QString &note, AuthProfile profile, QString* message, bool* ok );

bool HttpReqScreenRemove( QStringList scrensId, AuthProfile profile, QString* message, bool* ok );

#endif //ADAPTIXCLIENT_REQUESTOR_H
