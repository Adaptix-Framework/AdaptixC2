#include <Workers/DownloaderWorker.h>
#include <UI/Dialogs/DialogDownloader.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <Utils/FontManager.h>

#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThread>
#include <QGuiApplication>
#include <QClipboard>

static QString formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 b").arg(bytes);
    if (bytes < 1024*1024) return QString("%1 Kb").arg(bytes/1024.0, 0, 'f', 1);
    if (bytes < 1024*1024*1024) return QString("%1 Mb").arg(bytes/(1024.0*1024), 0, 'f', 1);
    return QString("%1 Gb").arg(bytes/(1024.0*1024*1024), 0, 'f', 1);
}

DialogDownloader::DialogDownloader(const QString &url, const QString &otp, const QString &savedPath, FilesFeedWidget *dw, bool autoHide, QWidget *parent) : QDialog(parent), downloadsWidget(dw), savedPath(savedPath)
{
    this->setWindowTitle("Download");
    this->setFixedWidth(420);
    this->setProperty("Main", "base");

    QFileInfo fi(savedPath);
    QString displayName = fi.fileName();

    const AppTypography& ty = FontManager::instance().typography();
    const QString monoFamily = ty.family;

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    auto* badge = new QLabel("\u2193 DL", this);
    badge->setStyleSheet(QStringLiteral("font-size:%1px;font-weight:600;padding:1px 6px;border-radius:3px;background:rgba(88,166,255,0.15);color:#58a6ff;").arg(ty.captionFontPx));
    headerLabel = new QLabel(displayName, this);
    headerLabel->setStyleSheet(QStringLiteral("font-size:%1px;font-weight:600;").arg(ty.titleFontPx));
    sizeLabel = new QLabel(this);
    sizeLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#8b949e;").arg(ty.chromeFontPx));
    sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(badge);
    headerLayout->addWidget(headerLabel, 1);
    headerLayout->addWidget(sizeLabel);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setFixedHeight(8);
    progressBar->setTextVisible(false);

    statsLabel = new QLabel(this);
    statsLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#8b949e;").arg(ty.chromeFontPx));

    auto* pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(8);
    pathEdit = new QLineEdit(savedPath, this);
    pathEdit->setReadOnly(true);
    pathEdit->setStyleSheet(QStringLiteral("font-size:%1px;font-family:'%2';").arg(ty.chromeFontPx).arg(monoFamily));
    copyButton = new QPushButton("Copy", this);
    copyButton->setStyleSheet(QStringLiteral("font-size:%1px;padding:3px 8px;").arg(ty.chromeFontPx));
    pathLayout->addWidget(pathEdit, 1);
    pathLayout->addWidget(copyButton);

    pathLabel = new QLabel("Saved to:", this);
    pathLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#8b949e;").arg(ty.chromeFontPx));
    pathLabel->setVisible(false);
    pathEdit->setVisible(false);
    copyButton->setVisible(false);

    hideButton = new QPushButton("Hide", this);
    cancelButton = new QPushButton("Cancel", this);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(hideButton);
    buttonLayout->addWidget(cancelButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statsLabel);
    mainLayout->addWidget(pathLabel);
    mainLayout->addLayout(pathLayout);
    mainLayout->addLayout(buttonLayout);

    if (!downloadsWidget)
        hideButton->setVisible(false);

    connect(copyButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(pathEdit->text());
        copyButton->setText("Copied!");
    });

    workerThread = new QThread(this);
    worker = new DownloaderWorker(url, otp, savedPath);
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &DownloaderWorker::start);
    connect(cancelButton, &QPushButton::clicked, worker, &DownloaderWorker::cancel);

    connect(hideButton, &QPushButton::clicked, this, [this]() {
        hidden = true;
        this->hide();
    });

    connect(worker, &DownloaderWorker::progress, this, [this](const qint64 received, const qint64 total) {
        currentReceived = received;
        currentTotal = total;

        if (!hidden) {
            int percent = (total > 0) ? static_cast<int>((received * 100) / total) : 0;
            progressBar->setValue(percent);
            sizeLabel->setText(formatSize(total));

            QString recvStr = formatSize(received);
            QString totalStr = formatSize(total);
            QString speedStr = "0 KB/s";
            statsLabel->setText(QString("%1 / %2  |  %3  |  %4%").arg(recvStr, totalStr, speedStr, QString::number(percent)));
        }

        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->UpdateSyncEntry(syncId, received, total, 0);
    });

    connect(worker, &DownloaderWorker::speedUpdated, this, [this](const double kbps) {
        if (!hidden) {
            int percent = (currentTotal > 0) ? static_cast<int>((currentReceived * 100) / currentTotal) : 0;
            QString speedStr = (kbps >= 1024) ? QString("%1 Mb/s").arg(kbps / 1024.0, 0, 'f', 1) : QString("%1 Kb/s").arg(kbps, 0, 'f', 1);
            statsLabel->setText(QString("%1 / %2  |  %3  |  %4%").arg(formatSize(currentReceived), formatSize(currentTotal), speedStr, QString::number(percent)));
        }

        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->UpdateSyncEntry(syncId, currentReceived, currentTotal, kbps);
    });

    connect(worker, &DownloaderWorker::finished, this, [this]() {
        if (downloadsWidget && !syncId.isEmpty()) {
            int state = worker->IsError() ? TRANSFER_STATE_CANCELED : TRANSFER_STATE_FINISHED;
            downloadsWidget->FinishSyncEntry(syncId, state);
            this->close();
            return;
        }

        if (!worker->IsError()) {
            progressBar->setValue(100);
            statsLabel->setText(QString("%1 / %2  |  Complete").arg(formatSize(currentTotal), formatSize(currentTotal)));

            progressBar->setVisible(false);
            pathLabel->setVisible(true);
            pathEdit->setVisible(true);
            copyButton->setVisible(true);
        }
        hideButton->setVisible(false);
        cancelButton->setText("Close");
        disconnect(cancelButton, nullptr, nullptr, nullptr);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::accept);
    });

    connect(worker, &DownloaderWorker::failed, this, [this](const QString &msg) {
        if (downloadsWidget && !syncId.isEmpty()) {
            downloadsWidget->FinishSyncEntry(syncId, TRANSFER_STATE_CANCELED);
            this->close();
            return;
        }

        statsLabel->setText(QString("Error: %1").arg(msg));
        statsLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#f85149;").arg(FontManager::instance().typography().chromeFontPx));
        hideButton->setVisible(false);
        cancelButton->setText("Close");
        disconnect(cancelButton, nullptr, nullptr, nullptr);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    });

    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    if (autoHide)
        hidden = true;

    if (downloadsWidget) {
        syncId = GenerateRandomString(8, "hex");
        SyncEntryData entry;
        entry.id = syncId;
        entry.direction = TRANSFER_DOWNLOAD;
        entry.filename = displayName;
        entry.localPath = savedPath;
        entry.timestamp = QDateTime::currentSecsSinceEpoch();
        entry.totalSize = 0;
        entry.progress = 0;
        entry.speed = 0;
        entry.state = TRANSFER_STATE_RUNNING;
        downloadsWidget->AddSyncEntry(entry);
    }

    workerThread->start();
}

DialogDownloader::~DialogDownloader()
{
    if (workerThread && workerThread->isRunning()) {
        if (worker)
            QMetaObject::invokeMethod(worker, "cancel", Qt::BlockingQueuedConnection);
        workerThread->quit();
        workerThread->wait(3000);
    }
}
