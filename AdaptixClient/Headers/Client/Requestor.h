#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/AuthProfile.h>

QJsonObject HttpReq( QString sUrl, QByteArray jsonData, QString token );

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

/// LISTENER

bool HttpReqListenerStart(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerEdit(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerStop( QString listenerName, QString listenerType, AuthProfile profile, QString* message, bool* ok );

/// AGENT

bool HttpReqAgentGenerate( QString listenerName, QString listenerType, QString agentName, QString configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentCommand( QString agentName, QString agentId, QString cmdLine, QString data, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentExit( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentRemove( QStringList agentsId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetTag( QStringList agentsId, QString tag, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTaskStop(QString agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTasksDelete(QString agentId, QStringList tasksId, AuthProfile profile, QString* message, bool* ok );

///DOWNLOAD

bool HttpReqBrowserDownload( QString action, QString fileId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserDownloadStart( QString agentId, QString path, AuthProfile profile, QString* message, bool* ok );

///BROWSER

bool HttpReqBrowserDisks( QString agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserProcess( QString agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserList( QString agentId, QString path, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserUpload( QString agentId, QString path, QString content, AuthProfile profile, QString* message, bool* ok );

///TUNNEL

bool HttpReqTunnelStop( QString tunnelId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqTunnelSetInfo( QString tunnelId, QString info, AuthProfile profile, QString* message, bool* ok );

#endif //ADAPTIXCLIENT_REQUESTOR_H
