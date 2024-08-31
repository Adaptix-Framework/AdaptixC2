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

