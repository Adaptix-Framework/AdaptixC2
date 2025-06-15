#include <Workers/DownloaderWorker.h>


DownloaderWorker::DownloaderWorker(const QUrl &url, const QString &otp, const QString &savedPath)
{
    this->savedPath = savedPath;
    this->otp = otp;
    this->url = QUrl(url);
    this->cancelled = false;
    this->error     = false;
    this->lastBytes = 0;
}

DownloaderWorker::~DownloaderWorker()
{
    if (networkReply)
        networkReply->deleteLater();
}

bool DownloaderWorker::IsError()
{
 return error;
}

void DownloaderWorker::start()
{
    this->networkManager = new QNetworkAccessManager(this);

    QNetworkRequest request(this->url);
    request.setRawHeader("OTP", this->otp.toUtf8());

    this->networkReply = this->networkManager->get(request);

    connect(this->networkReply, &QNetworkReply::readyRead, this, &DownloaderWorker::onReadyRead);
    connect(this->networkReply, &QNetworkReply::downloadProgress, this, &DownloaderWorker::onProgress);
    connect(this->networkReply, &QNetworkReply::finished, this, &DownloaderWorker::onFinished);
    connect(this->networkReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, &DownloaderWorker::onError);
    //     connect(this->networkReply, &QNetworkReply::errorOccurred,    this, &Downloader::onError);

    this->savedFile.setFileName(this->savedPath);
    if (!this->savedFile.open(QIODevice::WriteOnly)) {
        this->error = true;
        emit failed("Cannot open file: " + this->savedFile.errorString());
        return;
    }

    timer.start();
    lastBytes = 0;
}

void DownloaderWorker::cancel()
{
    this->cancelled = true;
    if (this->networkReply)
        this->networkReply->abort();
}

void DownloaderWorker::onReadyRead()
{
    if (!this->cancelled && this->savedFile.isOpen())
        this->savedFile.write(this->networkReply->readAll());
}

void DownloaderWorker::onProgress(const qint64 received, const qint64 total)
{
    emit progress(received, total);

    if (timer.elapsed() >= 1000) {
        double kbps = (received - lastBytes) / 1024.0;
        emit speedUpdated(kbps);
        lastBytes = received;
        timer.restart();
    }
}

void DownloaderWorker::onFinished()
{
    this->savedFile.close();
    if (this->cancelled) {
        this->savedFile.remove();
        emit failed("Download canceled.");
    } else {
        emit finished();
    }
}

void DownloaderWorker::onError(QNetworkReply::NetworkError)
{
    this->savedFile.close();
    this->savedFile.remove();
    emit failed("Download error: " + this->networkReply->errorString());
}