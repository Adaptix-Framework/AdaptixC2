#include <Client/AuthProfile.h>

AuthProfile::AuthProfile()
{
    this->valid = false;
}

AuthProfile::AuthProfile(const QString &project, const QString &username, const QString &password, const QString &host, const QString &port, const QString &endpoint, const QString &projectDir)
{
    this->project = project.trimmed();
    this->projectDir = QDir::fromNativeSeparators(projectDir.trimmed());
    this->username = username.trimmed();
    this->password = password;
    this->host = host.trimmed();
    this->port = port.trimmed();
    this->endpoint = endpoint.trimmed();
    this->valid = true;
}

AuthProfile::~AuthProfile() {}

QString AuthProfile::GetProject() { return this->project; };

QString AuthProfile::GetProjectDir() const
{
    QString dirPath = this->projectDir.trimmed();
    if (dirPath.isEmpty()) {
        QDir home(QDir::homePath());
        QString basePath = home.filePath("AdaptixProjects");
        QDir baseDir(basePath);
        dirPath = baseDir.filePath(this->project.trimmed());
    }

    QDir().mkpath(dirPath);
    return dirPath;
}

QString AuthProfile::GetUsername() { return this->username; };

QString AuthProfile::GetPassword() { return this->password; };

QString AuthProfile::GetHost() { return this->host; };

QString AuthProfile::GetPort() { return this->port; };

QString AuthProfile::GetEndpoint() { return this->endpoint; };

QString AuthProfile::GetAccessToken() { return this->accessToken; };

QString AuthProfile::GetRefreshToken() { return this->refreshToken; };

QString AuthProfile::GetURL() const
{
    return "https://" + host + ":" + port + endpoint;
};

void AuthProfile::SetAccessToken(const QString &token) { this->accessToken = token; };

void AuthProfile::SetRefreshToken(const QString &token) { this->refreshToken = token; }
