#ifndef ADAPTIXCLIENT_LASTTICKWORKER_H
#define ADAPTIXCLIENT_LASTTICKWORKER_H

#include <main.h>

class AdaptixWidget;

struct AgentMarkInfo {
    qint64  agentId;
    QString mark;
    QString lastMark;
};

class LastTickWorker : public QThread
{
Q_OBJECT
    AdaptixWidget* mainWidget = nullptr;
    QTimer*        timer      = nullptr;

public:
    explicit LastTickWorker(AdaptixWidget* w);
    ~LastTickWorker() override;

    void run() override;

Q_SIGNALS:
    void agentTickUpdate(const QList<AgentMarkInfo>& marks);

public Q_SLOTS:
    void updateLastItems();
    void stopWorker();
};

#endif
