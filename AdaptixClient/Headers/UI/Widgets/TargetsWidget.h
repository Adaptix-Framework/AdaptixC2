#ifndef TARGETSWIDGET_H
#define TARGETSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;
class ClickableLabel;

enum TargetsColumns {
    TRC_Id,
    TRC_Computer,
    TRC_Domain,
    TRC_Address,
    TRC_Tag,
    TRC_Os,
    TRC_Date,
    TRC_Info,
    TRC_ColumnCount
};



class TargetsFilterProxyModel : public QSortFilterProxyModel
{
    QString filter;
    bool    searchVisible = false;

public:
    explicit TargetsFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setSortRole(Qt::UserRole);
    };

    void setSearchVisible(bool visible) {
        if (searchVisible == visible) return;
        searchVisible = visible;
        invalidateFilter();
    }

    void setTextFilter(const QString &text){
        if (filter == text) return;
        filter = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(const int row, const QModelIndex &parent) const override {
        auto model = sourceModel();
        if (!model)
            return true;

        if (!searchVisible)
            return true;

        if (!filter.isEmpty()) {
            const int colCount = model->columnCount();
            QRegularExpression re(filter, QRegularExpression::CaseInsensitiveOption);
            bool matched = false;
            for (int col = 0; col < colCount; ++col) {
                QString val = model->index(row, col, parent).data().toString();
                if (val.contains(re)) {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return false;
        }

        return true;
    }
};



class TargetsTableModel : public QAbstractTableModel
{
    QVector<TargetData>   targets;
    QHash<QString, int>   idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < targets.size(); ++i)
            idToRow[targets[i].TargetId] = i;
    }

public:
    explicit TargetsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return targets.size(); }
    int columnCount(const QModelIndex&) const override { return TRC_ColumnCount; }

    QVariant data(const QModelIndex& index, const int role) const override {
        if (!index.isValid() || index.row() >= targets.size())
            return {};

        const TargetData& t = targets.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case TRC_Id:       return t.TargetId;
                case TRC_Computer: return t.Computer;
                case TRC_Domain:   return t.Domain;
                case TRC_Address:  return t.Address;
                case TRC_Tag:      return t.Tag;
                case TRC_Os:       return t.OsDesc;
                case TRC_Date:     return t.Date;
                case TRC_Info:     return t.Info;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case TRC_Date: return t.DateTimestamp;
                default:       return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case TRC_Address:
                case TRC_Date:
                    return Qt::AlignCenter;
            }
        }

        if (role == Qt::DecorationRole) {
            if (index.column() == TRC_Os) {
                return t.OsIcon;
            }
        }

        return {};
    }

    QVariant headerData(const int section, const Qt::Orientation o, const int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "Id","Computer","Domain","Address","Tag",
            "OS","Date","Info"
        };

        return headers.value(section);
    }

    void add(const TargetData& item) {
        const int row = targets.size();
        beginInsertRows(QModelIndex(), row, row);
        targets.append(item);
        idToRow[item.TargetId] = row;
        endInsertRows();
    }

    void add(const QList<TargetData>& list) {
        if (list.isEmpty())
            return;

        const int start = targets.size();
        const int end   = start + list.size() - 1;

        beginInsertRows(QModelIndex(), start, end);
        for (const auto& item : list) {
            idToRow[item.TargetId] = targets.size();
            targets.append(item);
        }
        endInsertRows();
    }

    void update(const QString& targetId, const TargetData& newTarget) {
        auto it = idToRow.find(targetId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        targets[row] = newTarget;
        Q_EMIT dataChanged(index(row, 0), index(row, TRC_ColumnCount - 1));
    }

    void remove(const QList<QString>& targetIds) {
        if (targetIds.isEmpty() || targets.isEmpty())
            return;

        QList<int> rowsToRemove;
        rowsToRemove.reserve(targetIds.size());

        for (const QString& id : targetIds) {
            auto it = idToRow.find(id);
            if (it != idToRow.end())
                rowsToRemove.append(it.value());
        }

        if (rowsToRemove.isEmpty())
            return;

        std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

        for (int row : rowsToRemove) {
            beginRemoveRows(QModelIndex(), row, row);
            idToRow.remove(targets[row].TargetId);
            targets.removeAt(row);
            endRemoveRows();
        }

        rebuildIndex();
    }

    void setTag(const QStringList &targetIds, const QString &tag) {
        if (targetIds.isEmpty() || targets.isEmpty())
            return;

        for (const QString& id : targetIds) {
            auto it = idToRow.find(id);
            if (it == idToRow.end())
                continue;

            int row = it.value();
            targets[row].Tag = tag;
            Q_EMIT dataChanged(index(row, TRC_Tag), index(row, TRC_Tag), {Qt::DisplayRole});
        }
    }

    void clear() {
        beginResetModel();
        targets.clear();
        idToRow.clear();
        endResetModel();
    }
};



class TargetsWidget : public DockTab
{
    AdaptixWidget* adaptixWidget  = nullptr;

    QGridLayout* mainGridLayout = nullptr;
    QTableView*  tableView      = nullptr;
    QShortcut*   shortcutSearch = nullptr;

    TargetsTableModel*       targetsModel = nullptr;
    TargetsFilterProxyModel* proxyModel   = nullptr;

    QWidget*        searchWidget = nullptr;
    QHBoxLayout*    searchLayout = nullptr;
    QLineEdit*      inputFilter  = nullptr;
    ClickableLabel* hideButton   = nullptr;

    void createUI();

public:
    explicit TargetsWidget(AdaptixWidget* w);
    ~TargetsWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddTargetsItems(QList<TargetData> targetList) const;
    void EditTargetsItem(const TargetData &newTarget) const;
    void RemoveTargetsItem(const QStringList &targetsId) const;
    void TargetsSetTag(const QStringList &targetIds, const QString &tag) const;

    void UpdateColumnsSize() const;
    void Clear() const;

    void TargetsAdd(QList<TargetData> targetList);

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleTargetsMenu( const QPoint &pos ) const;
    void onCreateTarget();
    void onEditTarget() const;
    void onRemoveTarget() const;
    void onSetTag() const;
    void onExportTarget() const;
    void onCopyToClipboard() const;
};

#endif //TARGETSWIDGET_H
