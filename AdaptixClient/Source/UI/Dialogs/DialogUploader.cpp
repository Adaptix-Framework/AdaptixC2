#include <UI/Dialogs/DialogUploader.h>
#include <Workers/UploaderWorker.h>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileInfo>

static void setupDialogUI(DialogUploader* dlg, QProgressBar*& progressBar, QLabel*& statusLabel, QLabel*& speedLabel, QPushButton*& cancelButton)
{
    dlg->setWindowTitle("正在上传...");
    dlg->resize(400, 150);
    dlg->setProperty("Main", "base");

    progressBar   = new QProgressBar(dlg);
    progressBar->setRange(0, 100);
    statusLabel   = new QLabel("准备上传...", dlg);
    speedLabel    = new QLabel("速度: 0 KB/s", dlg);
    cancelButton  = new QPushButton("取消", dlg);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->addWidget(statusLabel);
    layout->addWidget(progressBar);
    layout->addWidget(speedLabel);
    layout->addWidget(cancelButton);
    dlg->setLayout(layout);
}

static void connectWorkerSignals(DialogUploader* dlg, UploaderWorker* worker, QThread* workerThread,
    QProgressBar* progressBar, QLabel* statusLabel, QLabel* speedLabel, QPushButton* cancelButton)
{
    QObject::connect(workerThread, &QThread::started,     worker, &UploaderWorker::start);
    QObject::connect(cancelButton, &QPushButton::clicked, worker, &UploaderWorker::cancel);

    QObject::connect(worker, &UploaderWorker::progress, dlg, [progressBar, statusLabel](const qint64 sent, const qint64 total) {
        int percent = total > 0 ? static_cast<int>((sent * 100) / total) : 0;
        progressBar->setValue(percent);
        statusLabel->setText(QString("已上传 %1 / %2 MB").arg(sent / (1024 * 1024)).arg(total / (1024 * 1024)));
    });

    QObject::connect(worker, &UploaderWorker::speedUpdated, dlg, [speedLabel](const double kbps) {
        speedLabel->setText(QString("速度: %1 KB/s").arg(kbps, 0, 'f', 2));
    });

    QObject::connect(worker, &UploaderWorker::finished, dlg, [dlg, worker, progressBar, statusLabel, speedLabel]() {
        if (!worker->IsError()) {
            progressBar->setValue(100);
            statusLabel->setText("上传完成。");
            speedLabel->setVisible(false);
            Q_EMIT dlg->uploadFinished(true);
            dlg->accept();
            return;
        }
        Q_EMIT dlg->uploadFinished(false);
        dlg->reject();
    });

    QObject::connect(worker, &UploaderWorker::failed, dlg, [dlg](const QString &msg) {
        QMessageBox::critical(dlg, "上传错误", msg);
        Q_EMIT dlg->uploadFinished(false);
        dlg->reject();
    });

    QObject::connect(workerThread, &QThread::finished, worker,       &QObject::deleteLater);
    QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
}

DialogUploader::DialogUploader(const QUrl &uploadUrl, const QString &otp, const QByteArray &data, QWidget *parent) : QDialog(parent)
{
    setupDialogUI(this, progressBar, statusLabel, speedLabel, cancelButton);

    workerThread = new QThread(this);
    worker = new UploaderWorker(uploadUrl, otp, data);
    worker->moveToThread(workerThread);

    connectWorkerSignals(this, worker, workerThread, progressBar, statusLabel, speedLabel, cancelButton);
    workerThread->start();
}

DialogUploader::DialogUploader(const QUrl &uploadUrl, const QString &otp, const QString &filePath, QWidget *parent) : QDialog(parent)
{
    setupDialogUI(this, progressBar, statusLabel, speedLabel, cancelButton);

    QFileInfo fi(filePath);
    statusLabel->setText(QString("正在上传: %1 (%2 MB)").arg(fi.fileName()).arg(fi.size() / (1024 * 1024)));

    workerThread = new QThread(this);
    worker = new UploaderWorker(uploadUrl, otp, filePath);
    worker->moveToThread(workerThread);

    connectWorkerSignals(this, worker, workerThread, progressBar, statusLabel, speedLabel, cancelButton);
    workerThread->start();
}

DialogUploader::~DialogUploader()
{
    if (workerThread && workerThread->isRunning()) {
        worker->cancel();
        workerThread->quit();
        workerThread->wait();
    }
}
