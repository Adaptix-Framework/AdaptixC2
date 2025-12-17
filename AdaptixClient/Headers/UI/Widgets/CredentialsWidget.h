#ifndef CREDENTIALSWIDGET_H
#define CREDENTIALSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;
class ClickableLabel;

enum CredsColumns {
    CC_Id,
    CC_Username,
    CC_Password,
    CC_Realm,
    CC_Type,
    CC_Tag,
    CC_Date,
    CC_Storage,
    CC_Agent,
    CC_Host,
    CC_ColumnCount
};



class CredsFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    QString filter;
    bool    searchVisible = false;

public:
    explicit CredsFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
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



class CredsTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<CredentialData> creds;
    QHash<QString, int>     idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < creds.size(); ++i)
            idToRow[creds[i].CredId] = i;
    }

public:
    explicit CredsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return creds.size(); }
    int columnCount(const QModelIndex&) const override { return CC_ColumnCount; }

    QVariant data(const QModelIndex& index, const int role) const override {
        if (!index.isValid() || index.row() >= creds.size())
            return {};

        const CredentialData& c = creds.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case CC_Id:       return c.CredId;
                case CC_Username: return c.Username;
                case CC_Password: return c.Password;
                case CC_Realm:    return c.Realm;
                case CC_Type:     return c.Type;
                case CC_Tag:      return c.Tag;
                case CC_Date:     return c.Date;
                case CC_Storage:  return c.Storage;
                case CC_Agent:    return c.AgentId;
                case CC_Host:     return c.Host;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case CC_Date: return c.DateTimestamp;
                default:      return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case CC_Type:
                case CC_Date:
                case CC_Storage:
                case CC_Agent:
                    return Qt::AlignCenter;
            }
        }

        return {};
    }

    QVariant headerData(const int section, const Qt::Orientation o, const int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "CredId","Username","Password","Realm","Type",
            "Tag","Date","Storage","Agent","Host"
        };

        return headers.value(section);
    }

    void add(const CredentialData& item) {
        const int row = creds.size();
        beginInsertRows(QModelIndex(), row, row);
        creds.append(item);
        idToRow[item.CredId] = row;
        endInsertRows();
    }

    void add(const QList<CredentialData>& list) {
        if (list.isEmpty())
            return;

        const int start = creds.size();
        const int end   = start + list.size() - 1;

        beginInsertRows(QModelIndex(), start, end);
        for (const auto& item : list) {
            idToRow[item.CredId] = creds.size();
            creds.append(item);
        }
        endInsertRows();
    }

    void update(const QString& credId, const CredentialData& newCred) {
        auto it = idToRow.find(credId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        creds[row] = newCred;
        Q_EMIT dataChanged(index(row, 0), index(row, CC_ColumnCount - 1));
    }

    void remove(const QList<QString>& credIds) {
        if (credIds.isEmpty() || creds.isEmpty())
            return;

        QList<int> rowsToRemove;
        rowsToRemove.reserve(credIds.size());

        for (const QString& id : credIds) {
            auto it = idToRow.find(id);
            if (it != idToRow.end())
                rowsToRemove.append(it.value());
        }

        if (rowsToRemove.isEmpty())
            return;

        std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

        for (int row : rowsToRemove) {
            beginRemoveRows(QModelIndex(), row, row);
            idToRow.remove(creds[row].CredId);
            creds.removeAt(row);
            endRemoveRows();
        }

        rebuildIndex();
    }

    void setTag(const QStringList &credIds, const QString &tag) {
        if (credIds.isEmpty() || creds.isEmpty())
            return;

        for (const QString& id : credIds) {
            auto it = idToRow.find(id);
            if (it == idToRow.end())
                continue;

            int row = it.value();
            creds[row].Tag = tag;
            Q_EMIT dataChanged(index(row, CC_Tag), index(row, CC_Tag), {Qt::DisplayRole});
        }
    }

    void clear() {
        beginResetModel();
        creds.clear();
        idToRow.clear();
        endResetModel();
    }
};



class CredentialsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    CredsTableModel*       credsModel = nullptr;
    CredsFilterProxyModel* proxyModel = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    ClickableLabel* hideButton      = nullptr;

    void createUI();

public:
    explicit CredentialsWidget(AdaptixWidget* w);
    ~CredentialsWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddCredentialsItems(QList<CredentialData> credsList) const;
    void EditCredentialsItem(const CredentialData &newCredentials) const;
    void RemoveCredentialsItem(const QStringList &credsId) const;
    void CredsSetTag(const QStringList &credsIds, const QString &tag) const;

    void UpdateColumnsSize() const;
    void Clear() const;

    void CredentialsAdd(QList<CredentialData> credsList);

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleCredentialsMenu( const QPoint &pos ) const;
    void onCreateCreds();
    void onEditCreds() const;
    void onRemoveCreds() const;
    void onSetTag() const;
    void onExportCreds() const;
    void onCopyToClipboard() const;
};

#endif