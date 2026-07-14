#ifndef ADAPTIXCLIENT_GROUPINGPROXYMODEL_H
#define ADAPTIXCLIENT_GROUPINGPROXYMODEL_H

#include <main.h>
#include <QAbstractItemModel>
#include <QVector>
#include <QHash>
#include <QSet>

class AdaptixWidget;

enum ViewMode {
    VM_Flat,
    VM_CustomGroups,
    VM_AutoGroup,
    VM_Pivots,
};

enum AutoGroupField {
    AG_None,
    AG_ByDomain,
    AG_ByComputer,
    AG_ByUser,
    AG_ByTag,
    AG_ByListener,
    AG_ByOs,
    AG_ByAgentType,
    AG_ByRole,
};

struct GroupNode {
    int64_t         groupId        = 0;
    int64_t         parentGroupId  = 0;
    QString         name;
    bool            isAuto         = false;
    QVector<qint64> memberIds;
};

static const char* kAgentMimeType = "application/x-adaptix-agents";
static const char* kGroupMimeType = "application/x-adaptix-groups";

class GroupingProxyModel : public QAbstractItemModel
{
Q_OBJECT

    AdaptixWidget*      adaptixWidget = nullptr;

    QString             scope;
    QAbstractItemModel* source_model   = nullptr;
    ViewMode            view_mode      = VM_Flat;
    AutoGroupField      auto_field     = AG_None;
    int                 m_groupKeyRole = Qt::DisplayRole;

    struct DisplayGroup {
        GroupNode    node;
        QVector<int> sourceRows;
        int          parentGroupIdx = -1;   // -1 = top-level; >=0 = index in displayGroups_
        QVector<int> subGroupIndices;
    };

    QVector<DisplayGroup> display_groups;
    QVector<int>          ungroupedSourceRows;

    QVector<GroupNode> custom_groups;

    void rebuildHierarchy();
    void rebuildAutoGroups();
    void rebuildCustomGroups();
    void rebuildPivotTree();
    void remapSourceRows();
    void preserveExpandedRebuild();

    int findSourceRowForAgentId(qint64 agentId) const;
    qint64 agentIdAtSourceRow(int sourceRow) const;
    QString extractGroupKey(qint64 agentId) const;
    QString autoGroupPrefix() const;

    int  topLevelGroupCount() const;
    int  topLevelRowOfGroup(int globalIdx) const;
    int  groupGlobalIndexFromProxyIndex(const QModelIndex& index) const;
    bool wouldCreateCycle(int64_t groupId, int64_t newParentId) const;

    bool isTopLevel(const QModelIndex& idx) const { return idx.isValid() && idx.internalId() == 0; }
    int parentGroupIdx(const QModelIndex& idx) const { return static_cast<int>(idx.internalId()) - 1; }

public:
    explicit GroupingProxyModel(AdaptixWidget* w, const QString& scope, QObject* parent = nullptr);
    ~GroupingProxyModel() override = default;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    void setSourceModel(QAbstractItemModel* model);
    QAbstractItemModel* sourceModel() const;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;
    void setAutoGroupField(AutoGroupField field);
    AutoGroupField autoGroupField() const;
    void setGroupKeyRole(int role);

    bool isGroupIndex(const QModelIndex& index) const;
    int64_t groupIdFromIndex(const QModelIndex& index) const;
    qint64 agentIdFromIndex(const QModelIndex& index) const;
    QVector<GroupNode> allGroups() const;
    QVector<GroupNode> allCustomGroups() const;

    void addCustomGroup(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members);
    void renameCustomGroup(int64_t groupId, const QString& name);
    void deleteCustomGroup(int64_t groupId);
    void setCustomGroupMembers(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove);
    void removeAgentFromAllCustomGroups(qint64 agentId);
    void clearCustomGroups();
    void reparentCustomGroup(int64_t groupId, int64_t newParentId);

    void rebuild();

Q_SIGNALS:
    void groupStructureChanged();
    void agentsDroppedOnGroup(const QList<QPair<qint64,qint64>>& moves, qint64 toGroupId);
    void groupReparented(int64_t groupId, int64_t newParentId);

private Q_SLOTS:
    void onSourceRowsInserted(const QModelIndex &parent, int first, int last);
    void onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onSourceRowsRemoved(const QModelIndex &parent, int first, int last);
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles);
    void onSourceModelReset();
    void onSourceLayoutChanged();
};

#endif
