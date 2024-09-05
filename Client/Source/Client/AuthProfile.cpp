#include <Client/AuthProfile.h>

AuthProfile::AuthProfile() {
    this->valid = false;
}

AuthProfile::AuthProfile(QString project, QString username, QString password, QString host, QString port, QString endpoint) {
    this->project = project;
    this->username = username;
    this->password = password;
    this->host = host;
    this->port = port;
    this->endpoint = endpoint;
    this->valid = true;
}

AuthProfile::~AuthProfile() {}

QString AuthProfile::GetProject() { return this->project; };

QString AuthProfile::GetUsername() { return this->username; };

QString AuthProfile::GetPassword() { return this->password; };

QString AuthProfile::GetHost() { return this->host; };

QString AuthProfile::GetPort() { return this->port; };

QString AuthProfile::GetEndpoint() { return this->endpoint; };

QString AuthProfile::GetAccessToken() { return this->accessToken; };

void AuthProfile::SetAccessToken(QString token) { this->accessToken = token; };

void AuthProfile::SetRefreshToken(QString token) { this->refreshToken = token; };
