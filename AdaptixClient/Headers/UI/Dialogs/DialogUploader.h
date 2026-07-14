#ifndef DIALOGUPLOADER_H
#define DIALOGUPLOADER_H

#include <QDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QThread>

class UploaderWorker;
class FilesFeedWidget;

class DialogUploader : public QDialog {
Q_OBJECT
    QLabel*       headerLabel  = nullptr;
    QLabel*       sizeLabel    = nullptr;
    QProgressBar* progressBar  = nullptr;
    QLabel*       statsLabel   = nullptr;
    QPushButton*  hideButton   = nullptr;
    QPushButton*  cancelButton = nullptr;

    QThread*         workerThread = nullptr;
    UploaderWorker*  worker = nullptr;

    FilesFeedWidget* downloadsWidget = nullptr;
    QString          syncId;
    QString          displayName;
    QString          filePath;
    bool             hidden = false;
    qint64           currentSent  = 0;
    qint64           currentTotal = 0;

    void setupUI(const QString &name);
    void connectSignals();

public:
    explicit DialogUploader(const QUrl &uploadUrl, const QString &otp, const QByteArray &data, FilesFeedWidget *dw = nullptr, QWidget *parent = nullptr);
    explicit DialogUploader(const QUrl &uploadUrl, const QString &otp, const QString &filePath, FilesFeedWidget *dw = nullptr, QWidget *parent = nullptr);
    ~DialogUploader() override;

Q_SIGNALS:
    void uploadFinished(bool success);
};

#endif
