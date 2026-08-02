#ifndef ADAPTIXCLIENT_SESSIONSFEEDWIDGET_H
#define ADAPTIXCLIENT_SESSIONSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <Utils/CustomElements/GroupManagerPopup.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/SessionWidgetIface.h>
#include <UI/Models/GroupingProxyModel.h>
#include <UI/Models/AgentsFilterProxyModel.h>

#include <main.h>

class AdaptixWidget;
class Agent;

class SessionsFeedWidget : public ListFeedWidget, public SessionWidgetIface
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel* feedBlockModel = nullptr;
    AgentsFilterProxyModel* filterModel = nullptr;

    QPushButton*       btnGroupManager = nullptr;
    GroupManagerPopup* groupPopup      = nullptr;

    QHash<qint64, int> agentIdToRow;

    void rebuildAgentIndex();
    void setupCustomGroupsUi();
    void wireGroupingProxy(GroupingProxyModel* model);

public:
    explicit SessionsFeedWidget(AdaptixWidget* w);
    ~SessionsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock() override;
    void SetUpdatesEnabled(bool enabled) override;
    void AddAgentItem(Agent* agent) override;
    void UpdateAgentItem(const AgentData& oldData, const Agent* agent) override;
    void RemoveAgentItem(qint64 agentId) override;
    void UpdateData() override;
    void UpdateLastColumn(const QList<qint64>& agentIds) override;
    void UpdateColumnsVisible() override;
    void UpdateColumnsSize() override {}
    void UpdateAgentTypeComboBox() override;
    void Clear() override;
    QWidget* asWidget() override { return this; }

    void OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members) override;
    void OnGroupRenamed(int64_t groupId, const QString& name) override;
    void OnGroupDeleted(int64_t groupId) override;
    void OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove) override;
    void OnGroupReparented(int64_t groupId, int64_t newParentId) override;

    void onFilterChanged() override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);

private:
    void rebuildIdMap();
    void onItemDoubleClicked(const QModelIndex& index);

protected:
    void onGroupModeChanged(int index) override;
    void onSortingChanged(int index) override;
    void rebuildModelChain() override;

public Q_SLOTS:
    void actionConsoleOpen();
    void actionTasksOpen();
    void actionExecuteCommand();
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
