#ifndef ADAPTIXCLIENT_AGENT_H
#define ADAPTIXCLIENT_AGENT_H

#include <main.h>

class Commander;
class ConsoleWidget;
class BrowserFilesWidget;
class BrowserProcessWidget;
class TerminalContainerWidget;
class AdaptixWidget;
class AgentTableWidgetItem;
class GraphItem;

class Agent
{
public:
    AdaptixWidget* adaptixWidget = nullptr;

    AgentData data = {};

    QImage imageActive   = QImage();
    QImage imageInactive = QImage();
    QIcon  iconOs        = QIcon();

    QString LastMark   = QString();
    QString LastUpdate = QString();

    QString connType     = QString();
    QString listenerType = QString();
    QString parentId     = QString();
    QVector<QString> childsId;

    GraphItem* graphItem  = nullptr;
    QImage     graphImage = QImage();

    Commander*               commander      = nullptr;
    ConsoleWidget*           Console        = nullptr;

private:
    BrowserFilesWidget*      fileBrowser    = nullptr;
    BrowserProcessWidget*    processBrowser = nullptr;
    TerminalContainerWidget* terminal       = nullptr;
    TerminalContainerWidget* shell          = nullptr;

public:
    BrowserFilesWidget*      GetFileBrowser();
    BrowserProcessWidget*    GetProcessBrowser();
    TerminalContainerWidget* GetTerminal();
    TerminalContainerWidget* GetShell();

    bool HasFileBrowser() const    { return fileBrowser != nullptr; }
    bool HasProcessBrowser() const { return processBrowser != nullptr; }

    bool active = true;
    bool show   = true;
    QColor bg_color = QColor();
    QColor fg_color = QColor();

    explicit Agent(QJsonObject jsonObjAgentData, AdaptixWidget* w );
    ~Agent();

    void    Update(const QJsonObject &jsonObjAgentData);
    void    MarkItem(const QString &mark);
    void    UpdateImage();
    void TasksCancel(const QStringList &tasks) const;
    void TasksDelete(const QStringList &tasks) const;

    void SetParent(const PivotData &pivotData);
    void UnsetParent(const PivotData &pivotData);
    void AddChild(const PivotData &pivotData);
    void RemoveChild(const PivotData &pivotData);
};

#endif
