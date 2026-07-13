#ifndef DIALOGDOWNLOADER_H
#define DIALOGDOWNLOADER_H

#include <QLineEdit>
#include <QDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QThread>

class DownloaderWorker;
class FilesFeedWidget;

class DialogDownloader : public QDialog {
Q_OBJECT
    QLabel*       headerLabel  = nullptr;
    QLabel*       sizeLabel    = nullptr;
    QProgressBar* progressBar  = nullptr;
    QLabel*       statsLabel   = nullptr;
    QLabel*       pathLabel    = nullptr;
    QLineEdit*    pathEdit     = nullptr;
    QPushButton*  copyButton   = nullptr;
    QPushButton*  hideButton   = nullptr;
    QPushButton*  cancelButton = nullptr;

    QThread*          workerThread = nullptr;
    DownloaderWorker* worker       = nullptr;

    FilesFeedWidget* downloadsWidget = nullptr;
    QString          syncId;
    QString          savedPath;
    bool             hidden = false;
    qint64           currentReceived = 0;
    qint64           currentTotal    = 0;

public:
    explicit DialogDownloader(const QString &url, const QString &otp, const QString &savedPath, FilesFeedWidget *dw = nullptr, bool autoHide = false, QWidget *parent = nullptr);
    ~DialogDownloader() override;
};

#endif
