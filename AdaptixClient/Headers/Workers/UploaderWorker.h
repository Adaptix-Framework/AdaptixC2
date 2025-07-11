#ifndef UPLOADERWORKER_H
#define UPLOADERWORKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QElapsedTimer>
#include <QFile>

class UploaderWorker : public QObject {
Q_OBJECT
    QNetworkAccessManager* networkManager = nullptr;
    QNetworkReply*         networkReply   = nullptr;
    QElapsedTimer timer;
    QUrl url;
    QString otp;
    QByteArray data;
    bool cancelled = false;
    bool error = false;
    qint64 lastBytes = 0;

public:
    UploaderWorker(const QUrl &uploadUrl, const QString &otp, const QByteArray &data);
    ~UploaderWorker() override;

    bool IsError() const;

signals:
    void progress(qint64 sent, qint64 total);
    void speedUpdated(double kbps);
    void finished();
    void failed(const QString &error);

public slots:
    void start();
    void cancel();
    void onProgress(qint64, qint64);
    void onFinished();
    void onError(QNetworkReply::NetworkError);
};

#endif
