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

bool HttpReqAgentCommand( QString agentName, QString agentId, QString cmdLine, QString data, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentRemove( QString agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqAgentSetTag( QString agentId, QString tag, AuthProfile profile, QString* message, bool* ok );

///DOWNLOAD

bool HttpReqBrowserDownload( QString action, QString fileId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserDisks( QString agentId, AuthProfile profile, QString* message, bool* ok );

bool HttpReqBrowserList( QString agentId, QString path, AuthProfile profile, QString* message, bool* ok );

#endif //ADAPTIXCLIENT_REQUESTOR_H
