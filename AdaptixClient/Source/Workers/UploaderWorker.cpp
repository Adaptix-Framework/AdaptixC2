#include <Workers/UploaderWorker.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QUrlQuery>

UploaderWorker::UploaderWorker(const QUrl &uploadUrl, const QString &otp, const QByteArray &data) : url(uploadUrl), otp(otp), data(data), useFilePath(false) {}

UploaderWorker::UploaderWorker(const QUrl &uploadUrl, const QString &otp, const QString &filePath) : url(uploadUrl), otp(otp), filePath(filePath), useFilePath(true) {}

UploaderWorker::~UploaderWorker()
{
    if (networkReply)
        networkReply->deleteLater();
}

bool UploaderWorker::IsError() const
{
    return error;
}

void UploaderWorker::start() {
    networkManager = new QNetworkAccessManager(this);

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"command\""));

    if (useFilePath) {
        QFile* file = new QFile(filePath, multiPart);
        if (!file->open(QIODevice::ReadOnly)) {
            Q_EMIT failed("Failed to open file: " + filePath);
            return;
        }
        filePart.setBodyDevice(file);
    } else {
        filePart.setBody(data);
    }

    multiPart->append(filePart);

    QUrl requestUrl(url);
    QUrlQuery query;
    query.addQueryItem("otp", this->otp);
    requestUrl.setQuery(query);

    QNetworkRequest request(requestUrl);
    networkReply = networkManager->post(request, multiPart);
    multiPart->setParent(networkReply);

    connect(networkReply, &QNetworkReply::uploadProgress, this, &UploaderWorker::onProgress);
    connect(networkReply, &QNetworkReply::finished, this, &UploaderWorker::onFinished);
    connect(networkReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, &UploaderWorker::onError);

    timer.start();
    lastBytes = 0;
}

void UploaderWorker::cancel() {
    cancelled = true;
    if (networkReply)
        networkReply->abort();
}

void UploaderWorker::onProgress(const qint64 sent, const qint64 total) {
    Q_EMIT progress(sent, total);

    if (timer.elapsed() >= 1000) {
        double kbps = (sent - lastBytes) / 1024.0;
        Q_EMIT speedUpdated(kbps);
        lastBytes = sent;
        timer.restart();
    }
}

void UploaderWorker::onFinished() {
    if (cancelled) {
        Q_EMIT failed("Upload canceled.");
        return;
    }

    int statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode != 200) {
        QByteArray response = networkReply->readAll();
        Q_EMIT failed(QString("Server error: %1\nResponse: %2").arg(statusCode).arg(QString::fromUtf8(response)));
        return;
    }

    Q_EMIT finished();
}

void UploaderWorker::onError(QNetworkReply::NetworkError) {
    error = true;
    Q_EMIT failed("Upload error: " + networkReply->errorString());
}
