#ifndef ADAPTIXCLIENT_AGENT_H
#define ADAPTIXCLIENT_AGENT_H

#include <main.h>
#include <Agent/TableWidgetItemAgent.h>
#include <Agent/Commander.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/AdaptixWidget.h>

class ConsoleWidget;
class BrowserFilesWidget;
class BrowserProcessWidget;
class AdaptixWidget;

class Agent
{
public:
    AdaptixWidget* adaptixWidget = nullptr;

    bool    active = true;
    QString original_item_color;

    AgentData data   = {};

    TableWidgetItemAgent* item_Id       = nullptr;
    TableWidgetItemAgent* item_Type     = nullptr;
    TableWidgetItemAgent* item_Listener = nullptr;
    TableWidgetItemAgent* item_External = nullptr;
    TableWidgetItemAgent* item_Internal = nullptr;
    TableWidgetItemAgent* item_Domain   = nullptr;
    TableWidgetItemAgent* item_Computer = nullptr;
    TableWidgetItemAgent* item_Username = nullptr;
    TableWidgetItemAgent* item_Os       = nullptr;
    TableWidgetItemAgent* item_Process  = nullptr;
    TableWidgetItemAgent* item_Pid      = nullptr;
    TableWidgetItemAgent* item_Tid      = nullptr;
    TableWidgetItemAgent* item_Tags     = nullptr;
    TableWidgetItemAgent* item_Last     = nullptr;
    TableWidgetItemAgent* item_Sleep    = nullptr;

    ConsoleWidget*        Console        = nullptr;
    BrowserFilesWidget*   FileBrowser    = nullptr;
    BrowserProcessWidget* ProcessBrowser = nullptr;

    explicit Agent(QJsonObject jsonObjAgentData, Commander* commander, AdaptixWidget* w );
    ~Agent();

    void    Update(QJsonObject jsonObjAgentData);
    void    MarkItem(const QString &mark);
    void    SetColor(const QString &color) const;
    QString TasksStop(const QStringList &tasks) const;
    QString TasksDelete(const QStringList &tasks) const;

    QString BrowserDisks() const;
    QString BrowserProcess() const;
    QString BrowserList(const QString &path) const;
    QString BrowserUpload(const QString &path, const QString &content) const;
    QString BrowserDownload(const QString &path) const;
};

#endif //ADAPTIXCLIENT_AGENT_H
