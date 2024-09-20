#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/AuthProfile.h>

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqListenerStart( QString configName, QString configType, QString configData, AuthProfile profile, QString* answer, bool* ok );

#endif //ADAPTIXCLIENT_REQUESTOR_H
