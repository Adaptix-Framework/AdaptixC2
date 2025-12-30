#include <UI/Dialogs/DialogSyncPacket.h>

void CustomSplashScreen::mousePressEvent(QMouseEvent *event)
{
    event->ignore();
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

    logNameLabel = new QLabel("Log synchronization");

    logProgressLabel = new QLabel();
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar = new QProgressBar();

    cancelButton = new QPushButton("Cancel");
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
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar->setRange(0, totalLogs);
    progressBar->setValue(receivedLogs);
    cancelButton->setEnabled(true);
    cancelButton->setText("Cancel");
}

void DialogSyncPacket::upgrade()
{
    if (cancelled)
        return;

    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);

    if (totalLogs > 0) {
        progressBar->setValue(receivedLogs);
    }

    if (receivedLogs >= totalLogs) {
        finish();
    }
}

void DialogSyncPacket::finish()
{
    if (cancelled)
        return;

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
    double seconds = elapsed / 1000.0;

    QString completeMsg = QString("Synchronization complete! %1 items in %2s")
        .arg(totalLogs)
        .arg(seconds, 0, 'f', 2);

    logProgressLabel->setText(completeMsg);
    cancelButton->setText("Close");
    cancelButton->setEnabled(true);
    disconnect(cancelButton, &QPushButton::clicked, this, &DialogSyncPacket::onCancel);
    connect(cancelButton, &QPushButton::clicked, splashScreen, &QWidget::close);

    Q_EMIT syncFinished();

    QTimer::singleShot(500, splashScreen, &QWidget::close);
}

void DialogSyncPacket::error(const QString& message)
{
    logProgressLabel->setText("Error: " + message);
    progressBar->setValue(0);
    cancelButton->setText("Close");
    cancelButton->setEnabled(true);
    disconnect(cancelButton, &QPushButton::clicked, this, &DialogSyncPacket::onCancel);
    connect(cancelButton, &QPushButton::clicked, splashScreen, &QWidget::close);
}

void DialogSyncPacket::onCancel()
{
    if (cancelled)
        return;

    cancelled = true;
    logProgressLabel->setText("Cancelling...");
    cancelButton->setEnabled(false);
    Q_EMIT syncCancelled();
}
