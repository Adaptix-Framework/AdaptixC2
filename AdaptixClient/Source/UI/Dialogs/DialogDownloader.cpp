#include <Workers/DownloaderWorker.h>
#include <UI/Dialogs/DialogDownloader.h>

#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QThread>
#include <QGuiApplication>
#include <QScreen>

DialogDownloader::DialogDownloader(const QString &url, const QString &otp, const QString &savedPath, QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle("Synchronization...");
    this->resize(400, 150);
    this->setProperty("Main", "base");

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);

    statusLabel  = new QLabel("Starting...", this);
    speedLabel   = new QLabel("Speed: 0 KB/s", this);
    labelPath    = new QLabel("File saved to:", this);
    lineeditPath = new QLineEdit(savedPath, this);
    cancelButton = new QPushButton("Cancel", this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(statusLabel);
    layout->addWidget(progressBar);
    layout->addWidget(speedLabel);
    layout->addWidget(labelPath);
    layout->addWidget(lineeditPath);
    layout->addWidget(cancelButton);
    setLayout(layout);

    labelPath->setVisible(false);
    lineeditPath->setVisible(false);
    lineeditPath->setReadOnly(false);

    workerThread = new QThread(this);
    worker = new DownloaderWorker(url, otp, savedPath);
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started,     worker, &DownloaderWorker::start);
    connect(cancelButton, &QPushButton::clicked, worker, &DownloaderWorker::cancel);

    connect(worker, &DownloaderWorker::progress, this, [this](const qint64 received, const qint64 total) {
        int percent = (total > 0) ? static_cast<int>((received * 100) / total) : 0;
        progressBar->setValue(percent);

        QString recvStr, totalStr;
        if (total >= 1024 * 1024) {
            recvStr = QString::number(received / (1024.0 * 1024.0), 'f', 2) + " MB";
            totalStr = QString::number(total / (1024.0 * 1024.0), 'f', 2) + " MB";
        } else {
            recvStr = QString::number(received / 1024.0, 'f', 1) + " KB";
            totalStr = QString::number(total / 1024.0, 'f', 1) + " KB";
        }
        statusLabel->setText(QString("Downloaded %1 / %2").arg(recvStr).arg(totalStr));
    });

    connect(worker, &DownloaderWorker::speedUpdated, this, [this](const double kbps) {
        speedLabel->setText(QString("Speed: %1 KB/s").arg(kbps, 0, 'f', 2));
    });

    connect(worker, &DownloaderWorker::finished, this, [this]() {
        if (!worker->IsError()) {
            progressBar->setValue(100);
            statusLabel->setText("Download finished.");

            speedLabel->setVisible(false);
            labelPath->setVisible(true);
            lineeditPath->setVisible(true);
            lineeditPath->selectAll();
        }
        cancelButton->setText("Close");
        disconnect(cancelButton, nullptr, nullptr, nullptr);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::accept);
    });

    connect(worker, &DownloaderWorker::failed, this, [this](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
        this->reject();
    });

    connect(workerThread, &QThread::finished, worker,       &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    workerThread->start();
}

DialogDownloader::~DialogDownloader()
{
    if (workerThread && workerThread->isRunning()) {
        if (worker)
            QMetaObject::invokeMethod(worker, "cancel", Qt::QueuedConnection);
        workerThread->quit();
        workerThread->wait(3000);
    }
}
