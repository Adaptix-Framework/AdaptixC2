#ifndef ADAPTIXCLIENT_AUTHPROFILE_H
#define ADAPTIXCLIENT_AUTHPROFILE_H
#include <main.h>

class AuthProfile
{
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
    AuthProfile(const QString &project, const QString &username, const QString &password, const QString &host, const QString &port, const QString &endpoint);
    ~AuthProfile();

    QString GetProject();
    QString GetUsername();
    QString GetPassword();
    QString GetHost();
    QString GetPort();
    QString GetEndpoint();
    QString GetAccessToken();
    QString GetRefreshToken();
    QString GetURL() const;
    void    SetAccessToken(const QString &token);
    void    SetRefreshToken(const QString &token);
};

#endif //ADAPTIXCLIENT_AUTHPROFILE_H