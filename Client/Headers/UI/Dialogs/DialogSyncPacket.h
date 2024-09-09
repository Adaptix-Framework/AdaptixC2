#ifndef ADAPTIXCLIENT_DIALOGSYNCPACKET_H
#define ADAPTIXCLIENT_DIALOGSYNCPACKET_H

#include <main.h>

class DialogSyncPacket : public QDialog
{
    QLabel*       logNameLabel     = nullptr;
    QLabel*       logProgressLabel = nullptr;
    QProgressBar* progressBar      = nullptr;
    QVBoxLayout*  layout           = nullptr;

public:
    int totalLogs;
    int receivedLogs;

    explicit DialogSyncPacket(int total);
    ~DialogSyncPacket();

    void upgrade();
    void finish();
};

#endif //ADAPTIXCLIENT_DIALOGSYNCPACKET_H