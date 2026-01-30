#ifndef ADAPTIXCLIENT_DIALOGSYNCPACKET_H
#define ADAPTIXCLIENT_DIALOGSYNCPACKET_H

#include <main.h>

class DialogSyncPacket;

class CustomSplashScreen : public QSplashScreen
{
Q_OBJECT
    DialogSyncPacket* dialog = nullptr;

public:
    explicit CustomSplashScreen(DialogSyncPacket* d) : dialog(d) {}

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

class DialogSyncPacket : public QObject
{
Q_OBJECT

    QLabel*       logNameLabel       = nullptr;
    QLabel*       logProgressLabel   = nullptr;
    QProgressBar* progressBar        = nullptr;
    QLabel*       processProgressLabel = nullptr;
    QProgressBar* processProgressBar   = nullptr;
    QPushButton*  cancelButton       = nullptr;
    QVBoxLayout*  layout             = nullptr;

public:
    CustomSplashScreen* splashScreen = nullptr;
    int totalLogs    = 0;
    int receivedLogs = 0;
    qint64 startTime = 0;
    bool cancelled   = false;

    explicit DialogSyncPacket(QObject* parent = nullptr);
    ~DialogSyncPacket() override;

    void init(int count);
    void upgrade();
    void finish();
    void error(const QString& message);
    void setPhase(const QString& message, bool indeterminate);
    void setProcessingProgress(int batchIndex, int batchCount, int processed, int total);

Q_SIGNALS:
    void syncCancelled();
    void syncFinished();

public Q_SLOTS:
    void onCancel();
};

#endif