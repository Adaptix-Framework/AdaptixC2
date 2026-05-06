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

    bool matchesTerm(const QString &term, const QString &rowData) const {
        if (term.isEmpty())
            return true;
        QRegularExpression re(QRegularExpression::escape(term.trimmed()), QRegularExpression::CaseInsensitiveOption);
        return rowData.contains(re);
    }

    bool evaluateExpression(const QString &expr, const QString &rowData) const {
        QString e = expr.trimmed();
        if (e.isEmpty())
            return true;

        int depth = 0;
        int lastOr = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && c == '|') {
                lastOr = i;
                break;
            }
        }
        if (lastOr != -1) {
            QString left = e.left(lastOr).trimmed();
            QString right = e.mid(lastOr + 1).trimmed();
            return evaluateExpression(left, rowData) || evaluateExpression(right, rowData);
        }

        depth = 0;
        int lastAnd = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && c == '&') {
                lastAnd = i;
                break;
            }
        }
        if (lastAnd != -1) {
            QString left = e.left(lastAnd).trimmed();
            QString right = e.mid(lastAnd + 1).trimmed();
            return evaluateExpression(left, rowData) && evaluateExpression(right, rowData);
        }

        if (e.startsWith("^(") && e.endsWith(')')) {
            return !evaluateExpression(e.mid(2, e.length() - 3), rowData);
        }

        if (e.startsWith('(') && e.endsWith(')')) {
            return evaluateExpression(e.mid(1, e.length() - 2), rowData);
        }

        return matchesTerm(e, rowData);
    }

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
            QString rowData;
            rowData += model->index(row, TRC_Computer, parent).data().toString() + " ";
            rowData += model->index(row, TRC_Domain, parent).data().toString() + " ";
            rowData += model->index(row, TRC_Address, parent).data().toString() + " ";
            rowData += model->index(row, TRC_Tag, parent).data().toString() + " ";
            rowData += model->index(row, TRC_Os, parent).data().toString() + " ";
            rowData += model->index(row, TRC_Info, parent).data().toString() + " ";
            if (!evaluateExpression(filter, rowData))
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
                default: ;
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
                default: ;
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

        std::ranges::sort(rowsToRemove, std::greater<int>());

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

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    ClickableLabel* hideButton      = nullptr;

    bool bufferingEnabled = false;
    QList<TargetData> pendingTargets;

    void createUI();
    void flushPendingTargets();

public:
    explicit TargetsWidget(AdaptixWidget* w);
    ~TargetsWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddTargetsItems(QList<TargetData> targetList);
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
