#include <UI/Dialogs/DialogSyncPacket.h>

DialogSyncPacket::DialogSyncPacket(int total) {
    this->setWindowTitle("Sync data");
    this->setFixedSize(300, 150);

    logNameLabel = new QLabel("Log synchronization", this);

    receivedLogs = 0;
    totalLogs = total;
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel = new QLabel(progress, this);

    progressBar = new QProgressBar(this);
    progressBar->setRange(receivedLogs, totalLogs);
    progressBar->setValue(receivedLogs);

    layout = new QVBoxLayout(this);
    layout->addWidget(logNameLabel);
    layout->addWidget(logProgressLabel);
    layout->addWidget(progressBar);

    this->setLayout(layout);
    this->show();
}

DialogSyncPacket::~DialogSyncPacket() = default;

void DialogSyncPacket::upgrade() {
    QString progress = QString("Received: %1 / %2").arg(receivedLogs).arg(totalLogs);
    logProgressLabel->setText(progress);

    if (totalLogs > 0) {
        progressBar->setValue(receivedLogs);
    }

    if (receivedLogs >= totalLogs) {
        finish();
    }
}

void DialogSyncPacket::finish() {
    logProgressLabel->setText("Synchronization complete!");
    this->close();
}