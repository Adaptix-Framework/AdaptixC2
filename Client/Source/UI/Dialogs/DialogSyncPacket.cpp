#include <UI/Dialogs/DialogSyncPacket.h>

DialogSyncPacket::DialogSyncPacket(int total)
{
    splashScreen = new CustomSplashScreen();
    splashScreen->setPixmap(QPixmap(":/SyncLogo"));

    logNameLabel = new QLabel("Log synchronization");

    receivedLogs = 0;
    totalLogs = total;
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel = new QLabel(progress);
    logProgressLabel->setAlignment(Qt::AlignCenter);

    progressBar = new QProgressBar();
    progressBar->setRange(receivedLogs, totalLogs);
    progressBar->setValue(receivedLogs);

    layout = new QVBoxLayout(splashScreen);
    layout->addWidget(logNameLabel);
    layout->addStretch();
    layout->addWidget(progressBar);
    layout->addWidget(logProgressLabel);
    splashScreen->show();
}

DialogSyncPacket::~DialogSyncPacket() = default;

void DialogSyncPacket::upgrade()
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

void DialogSyncPacket::finish()
{
    logProgressLabel->setText("Synchronization complete!");
    splashScreen->close();
}