#ifndef ADAPTIXCLIENT_LASTTICKWORKER_H
#define ADAPTIXCLIENT_LASTTICKWORKER_H

#include <main.h>

class AdaptixWidget;

class LastTickWorker : public QThread
{
Q_OBJECT
    AdaptixWidget* mainWidget = nullptr;
    QTimer*        timer      = nullptr;

public:
    explicit LastTickWorker(AdaptixWidget* w);
    ~LastTickWorker() override;

    void run() override;

public Q_SLOTS:
    void updateLastItems() const;
};

#endif
