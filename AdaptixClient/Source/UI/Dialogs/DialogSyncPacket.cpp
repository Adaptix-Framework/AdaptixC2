#include <UI/Dialogs/DialogSyncPacket.h>

void CustomSplashScreen::mousePressEvent(QMouseEvent *event)
{
    event->ignore();
}

void DialogSyncPacket::setProcessingProgress(const int batchIndex, const int batchCount, const int processed, const int total)
{
    if (cancelled)
        return;

    if (total <= 0) {
        processProgressBar->setRange(0, 1);
        processProgressBar->setValue(0);
    } else {
        processProgressBar->setRange(0, total);
        processProgressBar->setValue(processed);
    }

    if (batchCount > 0)
        processProgressLabel->setText(QString("批处理 %1/%2: %3/%4").arg(batchIndex).arg(batchCount).arg(processed).arg(total));
    else
        processProgressLabel->setText(QString("处理中: %1/%2").arg(processed).arg(total));
}

void CustomSplashScreen::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && dialog) {
        dialog->onCancel();
    }
    event->ignore();
}

DialogSyncPacket::DialogSyncPacket(QObject* parent) : QObject(parent)
{
    splashScreen = new CustomSplashScreen(this);
    splashScreen->setPixmap(QPixmap(":/SyncLogo"));

    logNameLabel = new QLabel("数据同步");

    logProgressLabel = new QLabel();
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar = new QProgressBar();

    processProgressLabel = new QLabel();
    processProgressLabel->setAlignment(Qt::AlignCenter);

    processProgressBar = new QProgressBar();

    cancelButton = new QPushButton("取消");
    cancelButton->setFixedWidth(100);
    connect(cancelButton, &QPushButton::clicked, this, &DialogSyncPacket::onCancel);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    layout = new QVBoxLayout(splashScreen);
    layout->addWidget(logNameLabel);
    layout->addStretch();
    layout->addWidget(progressBar);
    layout->addWidget(logProgressLabel);
    layout->addWidget(processProgressBar);
    layout->addWidget(processProgressLabel);
    layout->addLayout(buttonLayout);
}

DialogSyncPacket::~DialogSyncPacket()
{
    if (splashScreen) {
        splashScreen->close();
        delete splashScreen;
    }
}

void DialogSyncPacket::init(int count)
{
    cancelled = false;
    receivedLogs = 0;
    totalLogs = count;
    startTime = QDateTime::currentMSecsSinceEpoch();
    QString progress = QString("已接收: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar->setRange(0, totalLogs);
    progressBar->setValue(receivedLogs);

    processProgressBar->setRange(0, 1);
    processProgressBar->setValue(0);
    processProgressLabel->setText("处理中: 0 / 0");

    cancelButton->setEnabled(true);
    cancelButton->setText("取消");
}

void DialogSyncPacket::upgrade()
{
    if (cancelled)
        return;

    QString progress = QString("已接收: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);

    if (totalLogs > 0)
        progressBar->setValue(receivedLogs);
}

void DialogSyncPacket::finish()
{
    if (cancelled)
        return;

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
    double seconds = elapsed / 1000.0;

    QString completeMsg = QString("同步完成！%1 项，耗时 %2 秒")
        .arg(totalLogs)
        .arg(seconds, 0, 'f', 2);

    logProgressLabel->setText(completeMsg);
    cancelButton->setText("关闭");
    cancelButton->setEnabled(true);
    disconnect(cancelButton, &QPushButton::clicked, this, &DialogSyncPacket::onCancel);
    connect(cancelButton, &QPushButton::clicked, splashScreen, &QWidget::close);

    Q_EMIT syncFinished();
}

void DialogSyncPacket::error(const QString& message)
{
    logProgressLabel->setText("错误：" + message);
    progressBar->setValue(0);
    cancelButton->setText("关闭");
    cancelButton->setEnabled(true);
    disconnect(cancelButton, &QPushButton::clicked, this, &DialogSyncPacket::onCancel);
    connect(cancelButton, &QPushButton::clicked, splashScreen, &QWidget::close);
}

void DialogSyncPacket::setPhase(const QString& message, bool indeterminate)
{
    if (cancelled)
        return;

    logProgressLabel->setText(message);

    if (indeterminate) {
        progressBar->setRange(0, 0);
    } else {
        progressBar->setRange(0, totalLogs);
        progressBar->setValue(receivedLogs);
    }
}

void DialogSyncPacket::onCancel()
{
    if (cancelled)
        return;

    cancelled = true;
    logProgressLabel->setText("正在取消...");
    cancelButton->setEnabled(false);
    Q_EMIT syncCancelled();
}
