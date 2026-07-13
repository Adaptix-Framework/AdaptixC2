#ifndef ADAPTIXCLIENT_SESSIONSFEEDWIDGET_H
#define ADAPTIXCLIENT_SESSIONSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Models/GroupingProxyModel.h>
#include <UI/Models/AgentsFilterProxyModel.h>

#include <main.h>

class AdaptixWidget;
class Agent;

class SessionsFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel* feedBlockModel = nullptr;
    AgentsFilterProxyModel* filterModel = nullptr;

    QHash<qint64, int> agentIdToRow;

    void rebuildAgentIndex();

public:
    explicit SessionsFeedWidget(AdaptixWidget* w);
    ~SessionsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();
    void SetUpdatesEnabled(bool enabled);
    void AddAgentItem(Agent* agent);
    void UpdateAgentItem(const AgentData& oldData, const Agent* agent);
    void RemoveAgentItem(qint64 agentId);
    void UpdateData();
    void UpdateLastColumn(const QList<qint64>& agentIds);
    void UpdateColumnsVisible();
    void UpdateColumnsSize() {}
    void UpdateAgentTypeComboBox();
    void Clear();

    void OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members);
    void OnGroupRenamed(int64_t groupId, const QString& name);
    void OnGroupDeleted(int64_t groupId);
    void OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove);
    void OnGroupReparented(int64_t groupId, int64_t newParentId);

    void onFilterChanged() override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);

private:
    void rebuildIdMap();
    void onItemDoubleClicked(const QModelIndex& index);

protected:
    void onGroupModeChanged(int index) override;
    void onSortingChanged(int index) override;

public Q_SLOTS:
    void actionConsoleOpen();
    void actionTasksOpen();
    void actionExecuteCommand();
    void actionConsoleDelete();
    void actionAgentRemove();
    void actionMarkActive();
    void actionMarkInactive();
    void actionRemove();
    void actionSetTag();
    void actionSetData();
    void actionHide();
    void actionItemsShowAll();
    void actionItemColor();
    void actionTextColor();
    void actionColorReset();
};

#endif
