#ifndef DOWNLOADERWORKER_H
#define DOWNLOADERWORKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QElapsedTimer>

class DownloaderWorker : public QObject {
Q_OBJECT
    QNetworkAccessManager* networkManager = nullptr;
    QNetworkReply*         networkReply   = nullptr;
    QElapsedTimer timer;
    QUrl    url;
    QString otp;
    QString savedPath;
    QFile   savedFile;
    bool    cancelled;
    bool    error;
    qint64  lastBytes;

public:
    DownloaderWorker(const QUrl &url, const QString &otp, const QString &savedPath);
    ~DownloaderWorker() override;

    bool IsError();

signals:
    void progress(qint64 received, qint64 total);
    void speedUpdated(double kbps);
    void finished();
    void failed(const QString &error);

public slots:
    void start();
    void cancel();
    void onReadyRead();
    void onProgress(qint64, qint64);
    void onFinished();
    void onError(QNetworkReply::NetworkError);
};

#endif //DOWNLOADERWORKER_H
