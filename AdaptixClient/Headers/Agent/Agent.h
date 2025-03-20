#ifndef ADAPTIXCLIENT_AGENT_H
#define ADAPTIXCLIENT_AGENT_H

#include <main.h>
#include <Agent/AgentTableWidgetItem.h>
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

    AgentData data = {};

    QString parentId = QString();
    QVector<QString> childsId;

    AgentTableWidgetItem* item_Id       = nullptr;
    AgentTableWidgetItem* item_Type     = nullptr;
    AgentTableWidgetItem* item_Listener = nullptr;
    AgentTableWidgetItem* item_External = nullptr;
    AgentTableWidgetItem* item_Internal = nullptr;
    AgentTableWidgetItem* item_Domain   = nullptr;
    AgentTableWidgetItem* item_Computer = nullptr;
    AgentTableWidgetItem* item_Username = nullptr;
    AgentTableWidgetItem* item_Os       = nullptr;
    AgentTableWidgetItem* item_Process  = nullptr;
    AgentTableWidgetItem* item_Pid      = nullptr;
    AgentTableWidgetItem* item_Tid      = nullptr;
    AgentTableWidgetItem* item_Tags     = nullptr;
    AgentTableWidgetItem* item_Last     = nullptr;
    AgentTableWidgetItem* item_Sleep    = nullptr;

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

    // QString BrowserFiles(const QString &path) const;
    QString BrowserDisks() const;
    QString BrowserProcess() const;
    QString BrowserList(const QString &path) const;
    QString BrowserUpload(const QString &path, const QString &content) const;
    QString BrowserDownload(const QString &path) const;
};

#endif //ADAPTIXCLIENT_AGENT_H
