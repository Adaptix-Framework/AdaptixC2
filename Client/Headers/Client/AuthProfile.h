#ifndef ADAPTIXCLIENT_AUTHPROFILE_H
#define ADAPTIXCLIENT_AUTHPROFILE_H
#include <main.h>

class AuthProfile {
    QString project;
    QString username;
    QString password;
    QString host;
    QString port;
    QString endpoint;
    QString accessToken;
    QString refreshToken;

public:
    bool valid;

    AuthProfile();
    AuthProfile(QString project, QString username, QString password, QString host, QString port, QString endpoint);
    ~AuthProfile();

    QString GetProject();
    QString GetUsername();
    QString GetPassword();
    QString GetHost();
    QString GetPort();
    QString GetEndpoint();
    QString GetAccessToken();
    void    SetAccessToken(QString token);
    void    SetRefreshToken(QString token);

};

#endif //ADAPTIXCLIENT_AUTHPROFILE_H
