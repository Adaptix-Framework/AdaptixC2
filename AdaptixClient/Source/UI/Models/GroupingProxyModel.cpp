#include <UI/Models/GroupingProxyModel.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>

#include <QMimeData>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

GroupingProxyModel::GroupingProxyModel(AdaptixWidget* w, const QString& scope, QObject* parent) : QAbstractItemModel(parent), adaptixWidget(w), scope(scope) {}

void GroupingProxyModel::setSourceModel(QAbstractItemModel* model)
{
    if (source_model) {
        disconnect(source_model, nullptr, this, nullptr);
    }

    source_model = model;

    if (source_model) {
        connect(source_model, &QAbstractItemModel::rowsInserted,         this, &GroupingProxyModel::onSourceRowsInserted);
        connect(source_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &GroupingProxyModel::onSourceRowsAboutToBeRemoved);
        connect(source_model, &QAbstractItemModel::rowsRemoved,          this, &GroupingProxyModel::onSourceRowsRemoved);
        connect(source_model, &QAbstractItemModel::dataChanged,          this, &GroupingProxyModel::onSourceDataChanged);
        connect(source_model, &QAbstractItemModel::modelReset,           this, &GroupingProxyModel::onSourceModelReset);
        connect(source_model, &QAbstractItemModel::layoutChanged,        this, &GroupingProxyModel::onSourceLayoutChanged);
    }

    beginResetModel();
    rebuildHierarchy();
    endResetModel();
}

QAbstractItemModel* GroupingProxyModel::sourceModel() const
{
    return source_model;
}



int GroupingProxyModel::topLevelGroupCount() const
{
    int count = 0;
    for (const auto& dg : display_groups)
        if (dg.parentGroupIdx == -1) count++;
    return count;
}

int GroupingProxyModel::topLevelRowOfGroup(int globalIdx) const
{
    int row = 0;
    for (int i = 0; i < display_groups.size(); ++i) {
        if (display_groups[i].parentGroupIdx == -1) {
            if (i == globalIdx) return row;
            row++;
        }
    }
    return -1;
}

int GroupingProxyModel::groupGlobalIndexFromProxyIndex(const QModelIndex& index) const
{
    quintptr id = index.internalId();
    if (id == 0) {
        int tlIdx = 0;
        for (int i = 0; i < display_groups.size(); ++i) {
            if (display_groups[i].parentGroupIdx == -1) {
                if (tlIdx == index.row()) return i;
                tlIdx++;
            }
        }
        return -1;
    }

    int parentGlobalIdx = static_cast<int>(id) - 1;
    if (parentGlobalIdx < 0 || parentGlobalIdx >= display_groups.size())
        return -1;

    const DisplayGroup& parentDg = display_groups[parentGlobalIdx];
    int row = index.row();
    if (row < parentDg.subGroupIndices.size())
        return parentDg.subGroupIndices[row];
    return -1;
}



QModelIndex GroupingProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (view_mode == VM_Flat)
        return createIndex(row, column, quintptr(0));

    if (!parent.isValid())
        return createIndex(row, column, quintptr(0));

    if (isGroupIndex(parent)) {
        int parentGlobalIdx = groupGlobalIndexFromProxyIndex(parent);
        if (parentGlobalIdx < 0)
            return {};

        return createIndex(row, column, quintptr(parentGlobalIdx + 1));
    }

    return {};
}

QModelIndex GroupingProxyModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || view_mode == VM_Flat)
        return {};

    quintptr id = child.internalId();
    if (id == 0)
        return {};

    int parentGlobalIdx = static_cast<int>(id) - 1;
    if (parentGlobalIdx < 0 || parentGlobalIdx >= display_groups.size())
        return {};

    const DisplayGroup& parentDg = display_groups[parentGlobalIdx];
    if (parentDg.parentGroupIdx == -1) {
        int tlRow = topLevelRowOfGroup(parentGlobalIdx);
        return createIndex(tlRow, 0, quintptr(0));
    } else {
        int grandParentIdx = parentDg.parentGroupIdx;
        int rowInGrandParent = display_groups[grandParentIdx].subGroupIndices.indexOf(parentGlobalIdx);
        return createIndex(rowInGrandParent, 0, quintptr(grandParentIdx + 1));
    }
}

int GroupingProxyModel::rowCount(const QModelIndex &parent) const
{
    if (!source_model)
        return 0;

    if (view_mode == VM_Flat) {
        if (parent.isValid())
            return 0;
        return source_model->rowCount();
    }

    if (!parent.isValid())
        return topLevelGroupCount() + ungroupedSourceRows.size();

    if (isGroupIndex(parent)) {
        int groupGlobalIdx = groupGlobalIndexFromProxyIndex(parent);
        if (groupGlobalIdx < 0 || groupGlobalIdx >= display_groups.size())
            return 0;
        const DisplayGroup& dg = display_groups[groupGlobalIdx];
        return dg.subGroupIndices.size() + dg.sourceRows.size();
    }

    return 0;
}

int GroupingProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (!source_model)
        return 0;
    return source_model->columnCount(QModelIndex());
}

QVariant GroupingProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !source_model)
        return {};

    if (view_mode == VM_Flat)
        return source_model->data(source_model->index(index.row(), index.column()), role);

    if (isGroupIndex(index)) {
        int groupGlobalIdx = groupGlobalIndexFromProxyIndex(index);
        if (groupGlobalIdx < 0 || groupGlobalIdx >= display_groups.size())
            return {};

        const DisplayGroup& dg = display_groups[groupGlobalIdx];

        if (index.column() == 0) {
            if (role == Qt::DisplayRole) {
                int loaded = dg.sourceRows.size();
                int total  = dg.node.memberIds.size();
                if (scope == "tasks" && total > loaded)
                    return QString("%1 (%2/%3)").arg(dg.node.name).arg(loaded).arg(total);
                return QString("%1 (%2)").arg(dg.node.name).arg(loaded);
            }
            if (role == Qt::FontRole) {
                QFont f;
                f.setBold(true);
                return f;
            }
        }

        if (view_mode == VM_Pivots && dg.node.groupId > 0) {
            int srcRow = findSourceRowForAgentId(dg.node.groupId);
            if (srcRow >= 0)
                return source_model->data(source_model->index(srcRow, index.column()), role);
        }

        return {};
    }

    QModelIndex srcIdx = mapToSource(index);
    if (srcIdx.isValid())
        return source_model->data(srcIdx, role);

    return {};
}

QVariant GroupingProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (source_model)
        return source_model->headerData(section, orientation, role);
    return {};
}

Qt::ItemFlags GroupingProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (view_mode == VM_CustomGroups) {
        if (isGroupIndex(index)) {
            int64_t gid = groupIdFromIndex(index);
            if (gid > 0)
                f |= Qt::ItemIsDragEnabled;
            f |= Qt::ItemIsDropEnabled;
        } else {
            f |= Qt::ItemIsDragEnabled;
        }
    }

    return f;
}

Qt::DropActions GroupingProxyModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList GroupingProxyModel::mimeTypes() const
{
    return { QString(kAgentMimeType), QString(kGroupMimeType) };
}

QMimeData* GroupingProxyModel::mimeData(const QModelIndexList &indexes) const
{
    if (view_mode != VM_CustomGroups)
        return nullptr;

    QJsonArray groupArr;
    QJsonArray agentArr;
    QSet<int64_t> seenGroups;
    QSet<qint64> seenAgents;

    for (const QModelIndex& idx : indexes) {
        if (idx.column() != 0)
            continue;

        if (isGroupIndex(idx)) {
            int64_t gid = groupIdFromIndex(idx);
            if (gid <= 0 || seenGroups.contains(gid))
                continue;
            seenGroups.insert(gid);

            int64_t fromParentId = 0;
            int gi = groupGlobalIndexFromProxyIndex(idx);
            if (gi >= 0 && gi < display_groups.size()) {
                int parentGIdx = display_groups[gi].parentGroupIdx;
                if (parentGIdx >= 0 && parentGIdx < display_groups.size())
                    fromParentId = display_groups[parentGIdx].node.groupId;
            }

            QJsonObject obj;
            obj["groupId"]      = QJsonValue::fromVariant(QVariant::fromValue(gid));
            obj["fromParentId"] = QJsonValue::fromVariant(QVariant::fromValue(fromParentId));
            groupArr.append(obj);
        } else {
            qint64 agentId = agentIdFromIndex(idx);
            if (agentId == 0 || seenAgents.contains(agentId))
                continue;
            seenAgents.insert(agentId);

            qint64 fromGroupId = -1;
            quintptr id = idx.internalId();
            if (id != 0) {
                int parentGlobalIdx = static_cast<int>(id) - 1;
                if (parentGlobalIdx >= 0 && parentGlobalIdx < display_groups.size())
                    fromGroupId = display_groups[parentGlobalIdx].node.groupId;
            }

            QJsonObject obj;
            obj["agentId"]     = QJsonValue::fromVariant(QVariant::fromValue(agentId));
            obj["fromGroupId"] = QJsonValue::fromVariant(QVariant::fromValue(fromGroupId));
            agentArr.append(obj);
        }
    }

    if (!groupArr.isEmpty()) {
        auto* mime = new QMimeData;
        mime->setData(kGroupMimeType, QJsonDocument(groupArr).toJson(QJsonDocument::Compact));
        return mime;
    }
    if (!agentArr.isEmpty()) {
        auto* mime = new QMimeData;
        mime->setData(kAgentMimeType, QJsonDocument(agentArr).toJson(QJsonDocument::Compact));
        return mime;
    }
    return nullptr;
}

static QModelIndex resolveGroupDropTarget(const GroupingProxyModel* model, const QModelIndex& parent)
{
    if (!parent.isValid())
        return {};
    if (model->isGroupIndex(parent))
        return parent;
    const QModelIndex p = model->parent(parent);
    if (p.isValid() && model->isGroupIndex(p))
        return p;
    return {};
}

bool GroupingProxyModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    Q_UNUSED(parent)
    if (action != Qt::MoveAction || view_mode != VM_CustomGroups)
        return false;
    if (!data)
        return false;
    return data->hasFormat(kAgentMimeType) || data->hasFormat(kGroupMimeType);
}

bool GroupingProxyModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    const QModelIndex targetGroup = resolveGroupDropTarget(this, parent);
    const qint64 toGroupId = targetGroup.isValid() ? groupIdFromIndex(targetGroup) : -1;

    if (data->hasFormat(kGroupMimeType)) {
        const int64_t toParentId = (toGroupId > 0) ? static_cast<int64_t>(toGroupId) : 0;
        QJsonArray arr = QJsonDocument::fromJson(data->data(kGroupMimeType)).array();
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            int64_t groupId = static_cast<int64_t>(obj.value(QStringLiteral("groupId")).toVariant().toLongLong());
            if (groupId != 0 && groupId != toParentId && !wouldCreateCycle(groupId, toParentId))
                Q_EMIT groupReparented(groupId, toParentId);
        }
        return true;
    }

    QJsonArray arr = QJsonDocument::fromJson(data->data(kAgentMimeType)).array();

    QList<QPair<qint64, qint64>> moves;
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        qint64 agentId     = obj.value(QStringLiteral("agentId")).toVariant().toLongLong();
        qint64 fromGroupId = obj.value(QStringLiteral("fromGroupId")).toVariant().toLongLong();
        if (agentId != 0 && fromGroupId != toGroupId)
            moves.append({agentId, fromGroupId});
    }

    if (!moves.isEmpty())
        Q_EMIT agentsDroppedOnGroup(moves, toGroupId);

    return true;
}

void GroupingProxyModel::sort(int column, Qt::SortOrder order)
{
    if (source_model)
        source_model->sort(column, order);
}



QModelIndex GroupingProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !source_model)
        return {};

    if (view_mode == VM_Flat) {
        return source_model->index(proxyIndex.row(), proxyIndex.column());
    }

    quintptr id = proxyIndex.internalId();

    if (id == 0) {
        int row = proxyIndex.row();
        int tlGroupCount = topLevelGroupCount();

        if (row < tlGroupCount) {
            if (view_mode == VM_Pivots) {
                int groupGlobalIdx = groupGlobalIndexFromProxyIndex(proxyIndex);
                if (groupGlobalIdx >= 0) {
                    int srcRow = findSourceRowForAgentId(display_groups[groupGlobalIdx].node.groupId);
                    if (srcRow >= 0)
                        return source_model->index(srcRow, proxyIndex.column());
                }
            }
            return {};
        }

        int ungroupedIdx = row - tlGroupCount;
        if (ungroupedIdx >= 0 && ungroupedIdx < ungroupedSourceRows.size()) {
            int srcRow = ungroupedSourceRows[ungroupedIdx];
            return source_model->index(srcRow, proxyIndex.column());
        }
        return {};
    }

    int parentGlobalIdx = static_cast<int>(id) - 1;
    if (parentGlobalIdx < 0 || parentGlobalIdx >= display_groups.size())
        return {};

    const DisplayGroup& dg = display_groups[parentGlobalIdx];
    int childRow = proxyIndex.row();
    int numSubGroups = dg.subGroupIndices.size();

    if (childRow < numSubGroups)
        return {};

    int agentChildRow = childRow - numSubGroups;
    if (agentChildRow >= 0 && agentChildRow < dg.sourceRows.size()) {
        int srcRow = dg.sourceRows[agentChildRow];
        return source_model->index(srcRow, proxyIndex.column());
    }
    return {};
}

QModelIndex GroupingProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
        return {};

    if (view_mode == VM_Flat)
        return createIndex(sourceIndex.row(), sourceIndex.column(), quintptr(0));

    int srcRow = sourceIndex.row();

    for (int g = 0; g < display_groups.size(); ++g) {
        const auto& dg = display_groups[g];
        int childIdx = dg.sourceRows.indexOf(srcRow);
        if (childIdx >= 0) {
            int proxyRow = dg.subGroupIndices.size() + childIdx;
            return createIndex(proxyRow, sourceIndex.column(), quintptr(g + 1));
        }
    }

    int uIdx = ungroupedSourceRows.indexOf(srcRow);
    if (uIdx >= 0)
        return createIndex(topLevelGroupCount() + uIdx, sourceIndex.column(), quintptr(0));

    return {};
}



void GroupingProxyModel::setViewMode(ViewMode mode)
{
    beginResetModel();
    view_mode = mode;
    rebuildHierarchy();
    endResetModel();

    Q_EMIT groupStructureChanged();
}

ViewMode GroupingProxyModel::viewMode() const
{
    return view_mode;
}

void GroupingProxyModel::setAutoGroupField(AutoGroupField field)
{
    auto_field = field;

    if (view_mode == VM_AutoGroup) {
        beginResetModel();
        rebuildHierarchy();
        endResetModel();
        Q_EMIT groupStructureChanged();
    }
}

void GroupingProxyModel::setGroupKeyRole(int role)
{
    m_groupKeyRole = role;
}

AutoGroupField GroupingProxyModel::autoGroupField() const
{
    return auto_field;
}



bool GroupingProxyModel::isGroupIndex(const QModelIndex& index) const
{
    if (!index.isValid() || view_mode == VM_Flat)
        return false;

    quintptr id = index.internalId();
    if (id == 0)
        return index.row() < topLevelGroupCount();

    int parentGlobalIdx = static_cast<int>(id) - 1;
    if (parentGlobalIdx < 0 || parentGlobalIdx >= display_groups.size())
        return false;
    return index.row() < display_groups[parentGlobalIdx].subGroupIndices.size();
}

int64_t GroupingProxyModel::groupIdFromIndex(const QModelIndex& index) const
{
    if (!isGroupIndex(index))
        return 0;
    int globalIdx = groupGlobalIndexFromProxyIndex(index);
    if (globalIdx < 0 || globalIdx >= display_groups.size())
        return 0;
    return display_groups[globalIdx].node.groupId;
}

qint64 GroupingProxyModel::agentIdFromIndex(const QModelIndex& index) const
{
    QModelIndex srcIdx = mapToSource(index);
    if (!srcIdx.isValid())
        return 0;
    return source_model->data(source_model->index(srcIdx.row(), 0), Qt::UserRole).toLongLong();
}

QVector<GroupNode> GroupingProxyModel::allGroups() const
{
    QVector<GroupNode> result;
    result.reserve(display_groups.size());
    for (const auto& dg : display_groups)
        result.append(dg.node);
    return result;
}

QVector<GroupNode> GroupingProxyModel::allCustomGroups() const
{
    return custom_groups;
}



void GroupingProxyModel::addCustomGroup(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members)
{
    if (groupId == 0)
        return;
    if (parentId != 0 && wouldCreateCycle(groupId, parentId))
        return;

    for (auto& existing : custom_groups) {
        if (existing.groupId == groupId) {
            existing.parentGroupId = parentId;
            existing.name = name;
            existing.isAuto = false;
            existing.memberIds = members;
            if (view_mode == VM_CustomGroups)
                preserveExpandedRebuild();
            Q_EMIT groupStructureChanged();
            return;
        }
    }

    GroupNode node;
    node.groupId = groupId;
    node.parentGroupId = parentId;
    node.name = name;
    node.isAuto = false;
    node.memberIds = members;
    custom_groups.append(node);

    if (view_mode == VM_CustomGroups) {
        preserveExpandedRebuild();
    }
    Q_EMIT groupStructureChanged();
}

void GroupingProxyModel::renameCustomGroup(int64_t groupId, const QString& name)
{
    for (auto& g : custom_groups) {
        if (g.groupId == groupId) {
            g.name = name;
            break;
        }
    }

    if (view_mode == VM_CustomGroups) {
        for (int i = 0; i < display_groups.size(); ++i) {
            if (display_groups[i].node.groupId == groupId) {
                display_groups[i].node.name = name;
                QModelIndex idx;
                if (display_groups[i].parentGroupIdx == -1) {
                    int tlRow = topLevelRowOfGroup(i);
                    if (tlRow >= 0)
                        idx = createIndex(tlRow, 0, quintptr(0));
                } else {
                    int grandParentIdx = display_groups[i].parentGroupIdx;
                    int rowInParent = display_groups[grandParentIdx].subGroupIndices.indexOf(i);
                    if (rowInParent >= 0)
                        idx = createIndex(rowInParent, 0, quintptr(grandParentIdx + 1));
                }
                if (idx.isValid())
                    Q_EMIT dataChanged(idx, idx, {Qt::DisplayRole});
                break;
            }
        }
    }
    Q_EMIT groupStructureChanged();
}

void GroupingProxyModel::deleteCustomGroup(int64_t groupId)
{
    int64_t reparentTo = 0;
    for (const auto& g : custom_groups) {
        if (g.groupId == groupId) {
            reparentTo = g.parentGroupId;
            break;
        }
    }

    for (auto& g : custom_groups) {
        if (g.parentGroupId == groupId)
            g.parentGroupId = reparentTo;
    }

    for (int i = 0; i < custom_groups.size(); ++i) {
        if (custom_groups[i].groupId == groupId) {
            custom_groups.removeAt(i);
            break;
        }
    }

    if (view_mode == VM_CustomGroups) {
        preserveExpandedRebuild();
    }
    Q_EMIT groupStructureChanged();
}

void GroupingProxyModel::setCustomGroupMembers(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove)
{
    for (auto& g : custom_groups) {
        if (g.groupId == groupId) {
            for (qint64 id : remove)
                g.memberIds.removeAll(id);
            for (qint64 id : add) {
                if (!g.memberIds.contains(id))
                    g.memberIds.append(id);
            }
            break;
        }
    }

    if (view_mode == VM_CustomGroups) {
        preserveExpandedRebuild();
    }
    Q_EMIT groupStructureChanged();
}

void GroupingProxyModel::removeAgentFromAllCustomGroups(qint64 agentId)
{
    bool changed = false;
    for (auto& g : custom_groups) {
        if (g.memberIds.removeAll(agentId) > 0)
            changed = true;
    }
    if (changed) {
        if (view_mode == VM_CustomGroups) {
            preserveExpandedRebuild();
        }
        Q_EMIT groupStructureChanged();
    }
}

void GroupingProxyModel::clearCustomGroups()
{
    custom_groups.clear();
    if (view_mode == VM_CustomGroups) {
        preserveExpandedRebuild();
    }
    Q_EMIT groupStructureChanged();
}



void GroupingProxyModel::rebuild()
{
    beginResetModel();
    rebuildHierarchy();
    endResetModel();
}

void GroupingProxyModel::rebuildHierarchy()
{
    display_groups.clear();
    ungroupedSourceRows.clear();

    if (!source_model)
        return;

    switch (view_mode) {
        case VM_Flat:
            break;
        case VM_AutoGroup:
            rebuildAutoGroups();
            break;
        case VM_CustomGroups:
            rebuildCustomGroups();
            break;
        case VM_Pivots:
            rebuildPivotTree();
            break;
    }
}

void GroupingProxyModel::rebuildAutoGroups()
{
    if (!source_model)
        return;

    QHash<QString, QVector<int>> buckets;
    int srcCount = source_model->rowCount();

    if (auto_field == AG_ByRole) {
        for (int row = 0; row < srcCount; ++row) {
            QString key = source_model->data(source_model->index(row, 0), m_groupKeyRole).toString();
            if (key.isEmpty()) key = "(empty)";
            buckets[key].append(row);
        }
    } else {
        if (!adaptixWidget) return;
        for (int row = 0; row < srcCount; ++row) {
            qint64 agentId = agentIdAtSourceRow(row);
            if (agentId == 0) continue;
            QString key = extractGroupKey(agentId);
            if (key.isEmpty()) key = "(empty)";
            buckets[key].append(row);
        }
    }

    QStringList keys = buckets.keys();
    keys.sort(Qt::CaseInsensitive);

    for (const QString& key : keys) {
        DisplayGroup dg;
        dg.node.groupId = -qHash(key);
        dg.node.name = (auto_field == AG_ByRole) ? key : (autoGroupPrefix() + key);
        dg.node.isAuto = true;
        dg.sourceRows = buckets[key];
        dg.parentGroupIdx = -1;
        if (auto_field != AG_ByRole) {
            for (int srcRow : dg.sourceRows)
                dg.node.memberIds.append(agentIdAtSourceRow(srcRow));
        }
        display_groups.append(dg);
    }
}

void GroupingProxyModel::rebuildCustomGroups()
{
    if (!source_model || !adaptixWidget)
        return;

    QSet<int> assignedRows;
    int srcCount = source_model->rowCount();

    QHash<int64_t, int> groupIdToIdx;

    for (const GroupNode& gn : custom_groups) {
        DisplayGroup dg;
        dg.node = gn;
        dg.parentGroupIdx = -1;

        for (qint64 memberId : gn.memberIds) {
            int srcRow = findSourceRowForAgentId(memberId);
            if (srcRow >= 0) {
                dg.sourceRows.append(srcRow);
                assignedRows.insert(srcRow);
            }
        }

        groupIdToIdx[gn.groupId] = display_groups.size();
        display_groups.append(dg);
    }

    for (int i = 0; i < display_groups.size(); ++i) {
        int64_t parentId = display_groups[i].node.parentGroupId;
        if (parentId != 0) {
            auto it = groupIdToIdx.find(parentId);
            if (it != groupIdToIdx.end()) {
                display_groups[i].parentGroupIdx = it.value();
                display_groups[it.value()].subGroupIndices.append(i);
            }
        }
    }

    for (int row = 0; row < srcCount; ++row) {
        if (!assignedRows.contains(row))
            ungroupedSourceRows.append(row);
    }
}

void GroupingProxyModel::rebuildPivotTree()
{
if (!source_model || !adaptixWidget)
        return;

    QHash<qint64, QVector<qint64>> parentToChildren;
    QSet<qint64> childAgents;

    for (const auto& pivot : adaptixWidget->Pivots) {
        parentToChildren[pivot.ParentAgentId].append(pivot.ChildAgentId);
        childAgents.insert(pivot.ChildAgentId);
    }

    int srcCount = source_model->rowCount();
    QSet<int> assignedRows;

    std::function<int(qint64, int)> buildGroup = [&](qint64 agentId, int parentGlobalIdx) -> int {
        int srcRow = findSourceRowForAgentId(agentId);
        if (srcRow >= 0)
            assignedRows.insert(srcRow);

        DisplayGroup dg;
        dg.node.groupId = agentId;
        dg.node.name = QString("Agent #%1").arg(agentId);
        dg.node.isAuto = true;
        dg.parentGroupIdx = parentGlobalIdx;

        int myGlobalIdx = display_groups.size();
        display_groups.append(dg);

        for (qint64 childId : parentToChildren[agentId]) {
            if (parentToChildren.contains(childId)) {
                int childGlobalIdx = buildGroup(childId, myGlobalIdx);
                display_groups[myGlobalIdx].subGroupIndices.append(childGlobalIdx);
            } else {
                int childSrcRow = findSourceRowForAgentId(childId);
                if (childSrcRow >= 0) {
                    display_groups[myGlobalIdx].sourceRows.append(childSrcRow);
                    display_groups[myGlobalIdx].node.memberIds.append(childId);
                    assignedRows.insert(childSrcRow);
                }
            }
        }

        return myGlobalIdx;
    };

    for (int row = 0; row < srcCount; ++row) {
        qint64 agentId = agentIdAtSourceRow(row);
        if (agentId == 0)
            continue;

        if (childAgents.contains(agentId))
            continue;

        if (parentToChildren.contains(agentId)) {
            buildGroup(agentId, -1);
        }
    }

    for (int row = 0; row < srcCount; ++row) {
        if (!assignedRows.contains(row))
            ungroupedSourceRows.append(row);
    }
}



void GroupingProxyModel::remapSourceRows()
{
    if (!source_model)
        return;

    for (auto& dg : display_groups) {
        QVector<int> newRows;
        newRows.reserve(dg.node.memberIds.size());
        for (qint64 memberId : dg.node.memberIds) {
            int srcRow = findSourceRowForAgentId(memberId);
            if (srcRow >= 0)
                newRows.append(srcRow);
        }
        dg.sourceRows = newRows;
    }

    ungroupedSourceRows.clear();
    QSet<int> assigned;
    for (const auto& dg : display_groups)
        for (int r : dg.sourceRows)
            assigned.insert(r);

    int srcCount = source_model->rowCount();
    for (int row = 0; row < srcCount; ++row)
        if (!assigned.contains(row))
            ungroupedSourceRows.append(row);
}



void GroupingProxyModel::preserveExpandedRebuild()
{
    const QModelIndexList persistent = persistentIndexList();

    struct Info {
        bool    valid;
        bool    isGroup;
        int64_t groupId;
        qint64  agentId;
        int     column;
    };
    QVector<Info> infos;
    infos.reserve(persistent.size());
    for (const QModelIndex& pi : persistent) {
        Info info;
        info.valid  = pi.isValid();
        info.column = pi.column();
        if (!info.valid) {
            info.isGroup = false;
            info.groupId = 0;
            info.agentId = 0;
        } else if (isGroupIndex(pi)) {
            info.isGroup = true;
            info.groupId = groupIdFromIndex(pi);
            info.agentId = 0;
        } else {
            info.isGroup = false;
            info.groupId = 0;
            info.agentId = agentIdFromIndex(pi);
        }
        infos.append(info);
    }

    Q_EMIT layoutAboutToBeChanged();
    rebuildHierarchy();

    for (int i = 0; i < persistent.size(); ++i) {
        const Info& info = infos[i];
        QModelIndex newIdx;

        if (info.valid) {
            if (info.isGroup) {
                for (int g = 0; g < display_groups.size(); ++g) {
                    if (display_groups[g].node.groupId != info.groupId)
                        continue;
                    if (display_groups[g].parentGroupIdx == -1) {
                        int tlRow = topLevelRowOfGroup(g);
                        if (tlRow >= 0)
                            newIdx = createIndex(tlRow, info.column, quintptr(0));
                    } else {
                        int gp = display_groups[g].parentGroupIdx;
                        int rowInParent = display_groups[gp].subGroupIndices.indexOf(g);
                        if (rowInParent >= 0)
                            newIdx = createIndex(rowInParent, info.column, quintptr(gp + 1));
                    }
                    break;
                }
            } else if (info.agentId != 0) {
                int srcRow = findSourceRowForAgentId(info.agentId);
                if (srcRow >= 0) {
                    QModelIndex proxyIdx = mapFromSource(source_model->index(srcRow, 0));
                    if (proxyIdx.isValid())
                        newIdx = createIndex(proxyIdx.row(), info.column, proxyIdx.internalId());
                }
            }
        }

        changePersistentIndex(persistent[i], newIdx);
    }

    Q_EMIT layoutChanged();
}



int GroupingProxyModel::findSourceRowForAgentId(qint64 agentId) const
{
    if (!source_model)
        return -1;

    int count = source_model->rowCount();
    for (int row = 0; row < count; ++row) {
        QModelIndex idx = source_model->index(row, 0);
        if (source_model->data(idx, Qt::UserRole).toLongLong() == agentId)
            return row;
    }
    return -1;
}

qint64 GroupingProxyModel::agentIdAtSourceRow(int sourceRow) const
{
    if (!source_model)
        return 0;
    QModelIndex idx = source_model->index(sourceRow, 0);
    return source_model->data(idx, Qt::UserRole).toLongLong();
}

QString GroupingProxyModel::autoGroupPrefix() const
{
    switch (auto_field) {
        case AG_ByDomain:    return QStringLiteral("Domain: ");
        case AG_ByComputer:  return QStringLiteral("Computer: ");
        case AG_ByUser:      return QStringLiteral("User: ");
        case AG_ByTag:       return QStringLiteral("Tag: ");
        case AG_ByListener:  return QStringLiteral("Listener: ");
        case AG_ByOs:        return QStringLiteral("OS: ");
        case AG_ByAgentType: return QStringLiteral("Agent: ");
        default:             return {};
    }
}

QString GroupingProxyModel::extractGroupKey(qint64 agentId) const
{
    if (!adaptixWidget)
        return {};

    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent)
        return {};

    switch (auto_field) {
        case AG_ByDomain:     return agent->data.Domain;
        case AG_ByComputer:   return agent->data.Computer;
        case AG_ByUser:       return agent->data.Username;
        case AG_ByTag:        return agent->data.Tags;
        case AG_ByListener:   return agent->data.Listener;
        case AG_ByOs:
            switch (agent->data.Os) {
            case OS_WINDOWS: return QStringLiteral("Windows");
            case OS_LINUX:   return QStringLiteral("Linux");
            case OS_MAC:     return QStringLiteral("MacOS");
            default:         return QStringLiteral("Unknown");
        }
        case AG_ByAgentType:  return agent->data.Name;
        default:              return {};
    }
}



void GroupingProxyModel::onSourceRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (view_mode == VM_Flat) {
        beginInsertRows(QModelIndex(), first, last);
        endInsertRows();
    } else {
        preserveExpandedRebuild();
    }
}

void GroupingProxyModel::onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (view_mode == VM_Flat) {
        beginRemoveRows(QModelIndex(), first, last);
    }
}

void GroupingProxyModel::onSourceRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (view_mode == VM_Flat) {
        endRemoveRows();
    } else {
        preserveExpandedRebuild();
    }
}

void GroupingProxyModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles)
{
    if (view_mode == VM_Flat) {
        QModelIndex proxyTopLeft = createIndex(topLeft.row(), topLeft.column(), quintptr(0));
        QModelIndex proxyBottomRight = createIndex(bottomRight.row(), bottomRight.column(), quintptr(0));
        Q_EMIT dataChanged(proxyTopLeft, proxyBottomRight, roles);
    } else if (view_mode == VM_AutoGroup) {
        preserveExpandedRebuild();
        } else {
            for (int srcRow = topLeft.row(); srcRow <= bottomRight.row(); ++srcRow) {
                QModelIndex proxyIdx = mapFromSource(source_model->index(srcRow, topLeft.column()));
                if (proxyIdx.isValid()) {
                    QModelIndex proxyEnd = createIndex(proxyIdx.row(), bottomRight.column(), proxyIdx.internalId());
                    Q_EMIT dataChanged(proxyIdx, proxyEnd, roles);
                }
            }
    }
}

void GroupingProxyModel::onSourceModelReset()
{
    beginResetModel();
    rebuildHierarchy();
    endResetModel();
}

void GroupingProxyModel::onSourceLayoutChanged()
{
    if (view_mode == VM_Flat) {
        Q_EMIT layoutChanged();
    } else if (view_mode == VM_AutoGroup) {
        preserveExpandedRebuild();
    } else {
        remapSourceRows();
    }
}

void GroupingProxyModel::reparentCustomGroup(int64_t groupId, int64_t newParentId)
{
    if (newParentId != 0 && wouldCreateCycle(groupId, newParentId))
        return;
    for (auto& g : custom_groups) {
        if (g.groupId == groupId) {
            g.parentGroupId = newParentId;
            break;
        }
    }
    if (view_mode == VM_CustomGroups)
        preserveExpandedRebuild();
    Q_EMIT groupStructureChanged();
}

bool GroupingProxyModel::wouldCreateCycle(int64_t groupId, int64_t newParentId) const
{
    if (newParentId == 0 || groupId == 0)
        return false;
    if (newParentId == groupId)
        return true;

    int64_t current = newParentId;
    QSet<int64_t> visited;
    while (current != 0) {
        if (visited.contains(current))
            break;
        visited.insert(current);
        int64_t parent = 0;
        for (const GroupNode& g : custom_groups) {
            if (g.groupId == current) {
                parent = g.parentGroupId;
                break;
            }
        }
        if (parent == groupId)
            return true;
        current = parent;
    }
    return false;
}
