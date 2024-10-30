#ifndef ADAPTIXCLIENT_AGENT_H
#define ADAPTIXCLIENT_AGENT_H

#include <main.h>
#include <Agent/TableWidgetItemAgent.h>

class Agent
{
public:
    AgentData data = {0};

    TableWidgetItemAgent* item_Id;
    TableWidgetItemAgent* item_Type;
    TableWidgetItemAgent* item_Listener;
    TableWidgetItemAgent* item_External;
    TableWidgetItemAgent* item_Internal;
    TableWidgetItemAgent* item_Domain;
    TableWidgetItemAgent* item_Computer;
    TableWidgetItemAgent* item_Username;
    TableWidgetItemAgent* item_Os;
    TableWidgetItemAgent* item_Process;
    TableWidgetItemAgent* item_Pid;
    TableWidgetItemAgent* item_Tid;
    TableWidgetItemAgent* item_Tags;
    TableWidgetItemAgent* item_Last;
    TableWidgetItemAgent* item_Sleep;

    explicit Agent(QJsonObject jsonObj);
    ~Agent();
};

#endif //ADAPTIXCLIENT_AGENT_H
