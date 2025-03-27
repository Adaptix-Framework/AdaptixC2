#ifndef ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H
#define ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H

#include <main.h>

class Agent;

class AgentTableWidgetItem final : public QTableWidgetItem
{
public:
    Agent* agent = nullptr;

    explicit AgentTableWidgetItem( const QString& text, Agent* agent );
    ~AgentTableWidgetItem() override;

    void RevertColor();
    void SetColor(QColor bg, QColor fg);
};

#endif //ADAPTIXCLIENT_TABLEWIDGETITEMAGENT_H
