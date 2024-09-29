#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/AuthProfile.h>

QJsonObject HttpReq( QString sUrl, QByteArray jsonData, QString token );

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqListenerStart(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerEdit(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok );

bool HttpReqListenerStop( QString listenerName, QString listenerType, AuthProfile profile, QString* message, bool* ok );


#endif //ADAPTIXCLIENT_REQUESTOR_H
