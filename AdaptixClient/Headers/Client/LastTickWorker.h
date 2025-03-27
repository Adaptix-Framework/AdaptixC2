#ifndef ADAPTIXCLIENT_LASTTICKWORKER_H
#define ADAPTIXCLIENT_LASTTICKWORKER_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>

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

public slots:
    void updateLastItems() const;
};

#endif //ADAPTIXCLIENT_LASTTICKWORKER_H
