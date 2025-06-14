#ifndef DIALOGDOWNLOADER_H
#define DIALOGDOWNLOADER_H

#include <QLineEdit>
#include <QDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <Workers/DownloaderWorker.h>

class DialogDownloader : public QDialog {
Q_OBJECT
    QProgressBar* progressBar  = nullptr;
    QPushButton*  cancelButton = nullptr;
    QLabel*       speedLabel   = nullptr;
    QLabel*       statusLabel  = nullptr;
    QLabel*       labelPath    = nullptr;
    QLineEdit*    lineeditPash = nullptr;

    QThread*          workerThread;
    DownloaderWorker* worker;

public:
    explicit DialogDownloader(const QString &url, const QString &otp, const QString &savedPath, QWidget *parent = nullptr);
    ~DialogDownloader() override;
};

#endif //DIALOGDOWNLOADER_H
