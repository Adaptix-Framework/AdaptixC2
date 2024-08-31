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

    QString GetProject() { return project; };
    QString GetUsername() { return username; };
    QString GetPassword() { return password; };
    QString GetHost() { return host; };
    QString GetPort() { return port; };
    QString GetEndpoint() { return endpoint; };
};


#endif //ADAPTIXCLIENT_AUTHPROFILE_H
