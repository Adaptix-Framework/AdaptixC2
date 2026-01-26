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
    QString projectDir;
    QStringList subscriptions;
    QStringList registeredCategories;
    bool consoleMultiuser = true;

public:
    bool valid;

    AuthProfile();
    AuthProfile(const QString &project, const QString &username, const QString &password, const QString &host, const QString &port, const QString &endpoint, const QString &projectDir = QString());
    ~AuthProfile();

    QString GetProject();
    QString GetProjectDir() const;
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
    QStringList GetSubscriptions() const;
    void    SetSubscriptions(const QStringList &subs);
    QStringList GetRegisteredCategories() const;
    void    SetRegisteredCategories(const QStringList &cats);
    bool    GetConsoleMultiuser() const;
    void    SetConsoleMultiuser(bool multiuser);
};

#endif