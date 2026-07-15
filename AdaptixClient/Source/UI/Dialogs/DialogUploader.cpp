#include <UI/Dialogs/DialogUploader.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <Workers/UploaderWorker.h>
#include <Utils/FontManager.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileInfo>

static QString formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 b").arg(bytes);
    if (bytes < 1024*1024) return QString("%1 Kb").arg(bytes/1024.0, 0, 'f', 1);
    if (bytes < 1024*1024*1024) return QString("%1 Mb").arg(bytes/(1024.0*1024), 0, 'f', 1);
    return QString("%1 Gb").arg(bytes/(1024.0*1024*1024), 0, 'f', 1);
}

void DialogUploader::setupUI(const QString &name)
{
    displayName = name;

    this->setWindowTitle("Upload");
    this->setFixedWidth(420);
    this->setProperty("Main", "base");

    const AppTypography& ty = FontManager::instance().typography();

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    auto* badge = new QLabel("\u2191 UL", this);
    badge->setStyleSheet(QStringLiteral("font-size:%1px;font-weight:600;padding:1px 6px;border-radius:3px;background:rgba(210,153,34,0.15);color:#d29922;").arg(ty.captionFontPx));
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
    mainLayout->addLayout(buttonLayout);

    if (!downloadsWidget)
        hideButton->setVisible(false);
}

void DialogUploader::connectSignals()
{
    connect(workerThread, &QThread::started, worker, &UploaderWorker::start);
    connect(cancelButton, &QPushButton::clicked, worker, &UploaderWorker::cancel);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    connect(hideButton, &QPushButton::clicked, this, [this]() {
        hidden = true;
        this->hide();
    });

    connect(worker, &UploaderWorker::progress, this, [this](const qint64 sent, const qint64 total) {
        currentSent = sent;
        currentTotal = total;
        if (!hidden) {
            int percent = total > 0 ? static_cast<int>((sent * 100) / total) : 0;
            progressBar->setValue(percent);
            sizeLabel->setText(formatSize(total));
            statsLabel->setText(QString("%1 / %2  |  %3%").arg(formatSize(sent), formatSize(total), QString::number(percent)));
        }
        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->UpdateSyncEntry(syncId, sent, total, 0);
    });

    connect(worker, &UploaderWorker::speedUpdated, this, [this](const double kbps) {
        if (!hidden) {
            int percent = currentTotal > 0 ? static_cast<int>((currentSent * 100) / currentTotal) : 0;
            QString speedStr = (kbps >= 1024) ? QString("%1 Mb/s").arg(kbps / 1024.0, 0, 'f', 1) : QString("%1 Kb/s").arg(kbps, 0, 'f', 1);
            statsLabel->setText(QString("%1 / %2  |  %3  |  %4%").arg(formatSize(currentSent), formatSize(currentTotal), speedStr, QString::number(percent)));
        }
        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->UpdateSyncEntry(syncId, currentSent, currentTotal, kbps);
    });

    connect(worker, &UploaderWorker::finished, this, [this]() {
        if (!worker->IsError()) {
            if (downloadsWidget && !syncId.isEmpty())
                downloadsWidget->FinishSyncEntry(syncId, TRANSFER_STATE_FINISHED);
            if (!hidden) {
                progressBar->setValue(100);
                progressBar->setVisible(false);
                statsLabel->setText(QString("%1 / %2  |  Complete").arg(formatSize(currentTotal), formatSize(currentTotal)));
                statsLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#3fb950;").arg(FontManager::instance().typography().chromeFontPx));
            }
            Q_EMIT uploadFinished(true);
            if (hidden) this->close();
            else this->accept();
            return;
        }

        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->FinishSyncEntry(syncId, TRANSFER_STATE_CANCELED);
        Q_EMIT uploadFinished(false);
        if (hidden) this->close();
        else this->reject();
    });

    connect(worker, &UploaderWorker::failed, this, [this](const QString &msg) {
        if (downloadsWidget && !syncId.isEmpty())
            downloadsWidget->FinishSyncEntry(syncId, TRANSFER_STATE_CANCELED);
        Q_EMIT uploadFinished(false);
        if (hidden) { this->close(); return; }
        statsLabel->setText(QString("Error: %1").arg(msg));
        statsLabel->setStyleSheet(QStringLiteral("font-size:%1px;color:#f85149;").arg(FontManager::instance().typography().chromeFontPx));
        hideButton->setVisible(false);
        cancelButton->setText("Close");
        disconnect(cancelButton, nullptr, nullptr, nullptr);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    });
}

DialogUploader::DialogUploader(const QUrl &uploadUrl, const QString &otp, const QByteArray &data, FilesFeedWidget *dw, QWidget *parent) : QDialog(parent), downloadsWidget(dw)
{
    setupUI("command data");

    workerThread = new QThread(this);
    worker = new UploaderWorker(uploadUrl, otp, data);
    worker->moveToThread(workerThread);

    connectSignals();

    if (downloadsWidget) {
        syncId = GenerateRandomString(8, "hex");
        SyncEntryData entry;
        entry.id = syncId;
        entry.direction = TRANSFER_UPLOAD;
        entry.filename = displayName;
        entry.localPath = QString();
        entry.timestamp = QDateTime::currentSecsSinceEpoch();
        entry.totalSize = 0;
        entry.progress = 0;
        entry.speed = 0;
        entry.state = TRANSFER_STATE_RUNNING;
        downloadsWidget->AddSyncEntry(entry);
    }

    workerThread->start();
}

DialogUploader::DialogUploader(const QUrl &uploadUrl, const QString &otp, const QString &filePath, FilesFeedWidget *dw, QWidget *parent) : QDialog(parent), downloadsWidget(dw), filePath(filePath)
{
    QFileInfo fi(filePath);
    setupUI(fi.fileName());

    workerThread = new QThread(this);
    worker = new UploaderWorker(uploadUrl, otp, filePath);
    worker->moveToThread(workerThread);

    connectSignals();

    if (downloadsWidget) {
        syncId = GenerateRandomString(8, "hex");
        SyncEntryData entry;
        entry.id = syncId;
        entry.direction = TRANSFER_UPLOAD;
        entry.filename = displayName;
        entry.localPath = filePath;
        entry.timestamp = QDateTime::currentSecsSinceEpoch();
        entry.totalSize = 0;
        entry.progress = 0;
        entry.speed = 0;
        entry.state = TRANSFER_STATE_RUNNING;
        downloadsWidget->AddSyncEntry(entry);
    }

    workerThread->start();
}

DialogUploader::~DialogUploader()
{
    if (workerThread && workerThread->isRunning()) {
        if (worker)
            QMetaObject::invokeMethod(worker, "cancel", Qt::BlockingQueuedConnection);
        workerThread->quit();
        workerThread->wait(3000);
    }
}
