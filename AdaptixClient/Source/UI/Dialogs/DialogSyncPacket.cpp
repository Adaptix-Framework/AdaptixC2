#include <UI/Dialogs/DialogSyncPacket.h>

DialogSyncPacket::DialogSyncPacket()
{
    splashScreen = new CustomSplashScreen();
    splashScreen->setPixmap(QPixmap(":/SyncLogo"));

    logNameLabel = new QLabel("Log synchronization");

    logProgressLabel = new QLabel();
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar = new QProgressBar();

    layout = new QVBoxLayout(splashScreen);
    layout->addWidget(logNameLabel);
    layout->addStretch();
    layout->addWidget(progressBar);
    layout->addWidget(logProgressLabel);
}

DialogSyncPacket::~DialogSyncPacket() = default;

void DialogSyncPacket::init(int count)
{
    receivedLogs = 0;
    totalLogs = count;
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar->setRange(receivedLogs, totalLogs);
    progressBar->setValue(receivedLogs);
}

void DialogSyncPacket::upgrade() const
{
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);

    if (totalLogs > 0) {
        progressBar->setValue(receivedLogs);
    }

    if (receivedLogs >= totalLogs) {
        finish();
    }
}

void DialogSyncPacket::finish() const
{
    logProgressLabel->setText("Synchronization complete!");
    splashScreen->close();
}
