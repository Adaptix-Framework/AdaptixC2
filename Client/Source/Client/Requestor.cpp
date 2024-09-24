#include <Client/Requestor.h>

bool HttpReqLogin(AuthProfile* profile)
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QJsonObject dataJson;
    dataJson["username"] = profile->GetUsername();
    dataJson["password"] = profile->GetPassword();
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString urlTemplate = "https://%1:%2%3/login";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );
    QUrl url(sUrl);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();

    bool result = false;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();

        QJsonParseError parseError;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data, &parseError);

        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject()) {
            QJsonObject jsonObject = jsonResponse.object();

            if (jsonObject.contains("access_token") && jsonObject.contains("refresh_token")) {
                profile->SetAccessToken( jsonObject["access_token"].toString() );
                profile->SetRefreshToken( jsonObject["refresh_token"].toString() );
                result = true;
            }
        }
    }
    reply->deleteLater();
    return result;
}

bool HttpReqListenerStart(QString listenerName, QString configType, QString configData, AuthProfile profile, QString* message, bool* ok )
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = configType;
    dataJson["config"] = configData;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString urlTemplate = "https://%1:%2%3/listener/start";
    QString sUrl = urlTemplate.arg( profile.GetHost() ).arg( profile.GetPort() ).arg( profile.GetEndpoint() );
    QUrl url(sUrl);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString bearerToken = "Bearer " + profile.GetAccessToken();
    request.setRawHeader("Authorization", bearerToken.toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();

    bool result = false;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();

        QJsonParseError parseError;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data, &parseError);

        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject()) {
            QJsonObject jsonObject = jsonResponse.object();

            if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
                *message = jsonObject["message"].toString();
                *ok = jsonObject["ok"].toBool();
                result = true;
            }
        }
    }
    reply->deleteLater();
    return result;
}

bool HttpReqListenerStop( QString listenerName, QString listenerType, AuthProfile profile, QString* message, bool* ok )
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    QJsonObject dataJson;
    dataJson["name"] = listenerName;
    dataJson["type"] = listenerType;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    QString urlTemplate = "https://%1:%2%3/listener/stop";
    QString sUrl = urlTemplate.arg( profile.GetHost() ).arg( profile.GetPort() ).arg( profile.GetEndpoint() );
    QUrl url(sUrl);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString bearerToken = "Bearer " + profile.GetAccessToken();
    request.setRawHeader("Authorization", bearerToken.toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();

    bool result = false;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();

        QJsonParseError parseError;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data, &parseError);

        if (parseError.error == QJsonParseError::NoError && jsonResponse.isObject()) {
            QJsonObject jsonObject = jsonResponse.object();

            if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
                *message = jsonObject["message"].toString();
                *ok = jsonObject["ok"].toBool();
                result = true;
            }
        }
    }
    reply->deleteLater();
    return result;
}