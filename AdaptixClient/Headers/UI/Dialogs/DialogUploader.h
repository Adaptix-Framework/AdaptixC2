#ifndef DIALOGUPLOADER_H
#define DIALOGUPLOADER_H

#include <QDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QThread>

class UploaderWorker;

class DialogUploader : public QDialog {
    Q_OBJECT

    QProgressBar* progressBar  = nullptr;
    QPushButton*  cancelButton = nullptr;
    QLabel*       speedLabel   = nullptr;
    QLabel*       statusLabel  = nullptr;

    QThread*         workerThread = nullptr;
    UploaderWorker*  worker = nullptr;

public:
    explicit DialogUploader(const QUrl &uploadUrl, const QString &otp, const QByteArray &data, QWidget *parent = nullptr);
    ~DialogUploader() override;
};

#endif
