#ifndef ADAPTIXCLIENT_DIALOGSYNCPACKET_H
#define ADAPTIXCLIENT_DIALOGSYNCPACKET_H

#include <main.h>

class CustomSplashScreen : public QSplashScreen {
Q_OBJECT

protected:
    void mousePressEvent(QMouseEvent *event) override {
        event->ignore();
    }

    void keyPressEvent(QKeyEvent *event) override {
        event->ignore();
    }
};

class DialogSyncPacket
{
    QLabel*       logNameLabel       = nullptr;
    QLabel*       logProgressLabel   = nullptr;
    QProgressBar* progressBar        = nullptr;
    QVBoxLayout*  layout             = nullptr;
    CustomSplashScreen* splashScreen = nullptr;

public:
    int totalLogs;
    int receivedLogs;

    explicit DialogSyncPacket(int total);
    ~DialogSyncPacket();

    void upgrade();
    void finish();
};

#endif //ADAPTIXCLIENT_DIALOGSYNCPACKET_H