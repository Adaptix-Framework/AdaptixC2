#ifndef ADAPTIXCLIENT_AGENT_H
#define ADAPTIXCLIENT_AGENT_H

#include <main.h>
#include <Agent/TableWidgetItemAgent.h>
#include <Agent/Commander.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/AdaptixWidget.h>

class ConsoleWidget;
class BrowserFilesWidget;
class AdaptixWidget;

class Agent
{
public:
    AdaptixWidget* adaptixWidget = nullptr;

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

    ConsoleWidget*      Console     = nullptr;
    BrowserFilesWidget* FileBrowser = nullptr;

    explicit Agent(QJsonObject jsonObjAgentData, Commander* commander, AdaptixWidget* w );
    ~Agent();

    void    Update(QJsonObject jsonObjAgentData);
    QString BrowserDisks();
    QString BrowserList(QString path);
    QString BrowserUpload(QString path, QString content);
    QString BrowserDownload(QString path);
};

#endif //ADAPTIXCLIENT_AGENT_H
