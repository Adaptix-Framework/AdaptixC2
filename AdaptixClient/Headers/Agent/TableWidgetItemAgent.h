#ifndef ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H
#define ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H

#include <main.h>

class Agent;

class TableWidgetItemAgent final : public QTableWidgetItem
{
public:
    Agent* agent  = nullptr;

    explicit TableWidgetItemAgent( const QString& text, Agent* agent );
    ~TableWidgetItemAgent() override;
};

#endif //ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H
