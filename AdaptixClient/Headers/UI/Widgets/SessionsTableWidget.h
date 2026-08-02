#ifndef ADAPTIXCLIENT_SESSIONSTABLEWIDGET_H
#define ADAPTIXCLIENT_SESSIONSTABLEWIDGET_H

#include <main.h>
#include <MainAdaptix.h>
#include <Utils/FilterExpression.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/SessionWidgetIface.h>
#include <UI/Models/GroupingProxyModel.h>
#include <UI/Models/AgentsFilterProxyModel.h>
#include <Utils/CustomElements/GroupManagerPopup.h>
#include <Utils/CustomElements/ClickableLabel.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <oclero/qlementine/widgets/LineEdit.hpp>
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>
#include <Client/Settings.h>

#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QAbstractTableModel>
#include <QToolButton>

class Agent;
class AdaptixWidget;

class AgentsTableModel : public QAbstractTableModel
{
Q_OBJECT
    AdaptixWidget*     adaptixWidget;
    QVector<qint64>    agentsId;
    QHash<qint64, int> idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < agentsId.size(); ++i)
            idToRow[agentsId[i]] = i;
    }

public:
    explicit AgentsTableModel(AdaptixWidget* w, QObject* parent = nullptr) : QAbstractTableModel(parent), adaptixWidget(w) {}

    int rowCount(const QModelIndex&) const override {
        return agentsId.size();
    }

    int columnCount(const QModelIndex&) const override {
        return SC_ColumnCount;
    }

    QVariant data(const QModelIndex &index, const int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    void add(qint64 agentId) {
        const int row = agentsId.size();

        beginInsertRows(QModelIndex(), row, row);
        agentsId.append(agentId);
        idToRow[agentId] = row;
        endInsertRows();
    }

    void add(const QList<qint64> &ids) {
        if (ids.isEmpty())
            return;

        const int start = agentsId.size();
        const int end   = start + ids.size() - 1;

        beginInsertRows(QModelIndex(), start, end);
        for (qint64 id : ids) {
            idToRow[id] = agentsId.size();
            agentsId.append(id);
        }
        endInsertRows();
    }

    void update(qint64 agentId) {
        auto it = idToRow.find(agentId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        Q_EMIT dataChanged(index(row, 0), index(row, SC_ColumnCount - 1), { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole });
    }

    void updateColumn(qint64 agentId, int col) {
        auto it = idToRow.find(agentId);
        if (it == idToRow.end())
            return;
        int row = it.value();
        Q_EMIT dataChanged(index(row, col), index(row, col), { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole });
    }

    void updateLastColumn(const QList<qint64> &agentIds) {
        if (agentIds.isEmpty())
            return;

        int minRow = INT_MAX;
        int maxRow = -1;

        for (qint64 agentId : agentIds) {
            auto it = idToRow.find(agentId);
            if (it == idToRow.end())
                continue;

            int row = it.value();
            if (row < minRow) minRow = row;
            if (row > maxRow) maxRow = row;
        }

        if (maxRow >= 0) {
            Q_EMIT dataChanged(index(minRow, SC_Last), index(maxRow, SC_Last), { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole });
        }
    }

    void remove(qint64 agentId) {
        auto it = idToRow.find(agentId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        idToRow.remove(agentId);
        agentsId.removeAt(row);
        endRemoveRows();

        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        agentsId.clear();
        idToRow.clear();
        endResetModel();
    }

    void refreshAll() {
        if (agentsId.isEmpty())
            return;
        Q_EMIT dataChanged(
            index(0, 0),
            index(agentsId.size() - 1, SC_ColumnCount - 1),
            { Qt::DisplayRole, Qt::UserRole, Qt::ForegroundRole, Qt::BackgroundRole, Qt::DecorationRole, Qt::ToolTipRole });
    }
};

class SessionsTableWidget : public DockTab, public SessionWidgetIface
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;

    QGridLayout*  mainGridLayout = nullptr;
    ColorAwareTreeView* tableView = nullptr;
    QMenu*        menuSessions   = nullptr;
    QShortcut*    shortcutSearch = nullptr;

    AgentsFilterProxyModel* proxyModel    = nullptr;
    GroupingProxyModel*     groupingModel = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    QComboBox*      comboAgentType  = nullptr;
    QComboBox*      comboGroupBy    = nullptr;
    QToolButton*    checkOnlyActive  = nullptr;
    ClickableLabel* hideButton       = nullptr;
    QPushButton*    btnGroupManager  = nullptr;
    GroupManagerPopup* groupPopup    = nullptr;
    oclero::qlementine::LineEdit* inputFilter = nullptr;

    mutable bool columnStateReady = false;
    mutable bool columnsSizedOnce = false;
    bool bufferingEnabled = false;
    QList<Agent*> pendingAgents;

    void createUI();
    void flushPendingAgents();

public:
    AgentsTableModel* agentsModel = nullptr;
    explicit SessionsTableWidget( AdaptixWidget* w );
    ~SessionsTableWidget() override;

    void OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members) override;
    void OnGroupRenamed(int64_t groupId, const QString& name) override;
    void OnGroupDeleted(int64_t groupId) override;
    void OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove) override;
    void OnGroupReparented(int64_t groupId, int64_t newParentId) override;

    KDDockWidgets::QtWidgets::DockWidget* dock() override { return this->dockWidget; }

    void SetUpdatesEnabled(const bool enabled) override;

    void AddAgentItem(Agent* newAgent) override;
    void UpdateAgentItem(const AgentData &oldDatam, const Agent* agent) override;
    void RemoveAgentItem(qint64 agentId) override;

    void UpdateColumnsVisible() override;
    void UpdateColumnsSize() override;
    void UpdateLastColumn(const QList<qint64>& agentIds) override;
    void UpdateData() override;
    void UpdateAgentTypeComboBox() override;
    void Clear() override;
    QWidget* asWidget() override { return this; }

    void RestoreColumnState() const;
    void SaveColumnOrder() const;
    void AutoFitColumnToContents(int logicalIndex) const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterChanged() const;
    void onGroupModeChanged(int index);
    void handleTableDoubleClicked( const QModelIndex &index ) const;
    void handleSessionsTableMenu(const QPoint &pos );

    void actionConsoleOpen() const;
    void actionExecuteCommand();
    void actionTasksBrowserOpen() const;
    void actionMarkActive() const;
    void actionMarkInactive() const;
    void actionItemColor() const;
    void actionTextColor() const;
    void actionColorReset() const;
    void actionAgentRemove();
    void actionItemTag() const;
    void actionItemHide();
    void actionItemsShowAll();
    void actionSetData() const;
};

#endif
