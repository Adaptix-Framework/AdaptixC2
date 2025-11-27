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

    void add(const QList<CredentialData>& list) {
        if (list.isEmpty())
            return;

        const int start = creds.size();
        const int end   = start + list.size() - 1;

        beginInsertRows(QModelIndex(), start, end);
        creds.append(list);
        endInsertRows();
    }


    void update(const QString& credId, const CredentialData& newCred) {
        for (int i = 0; i < creds.size(); ++i) {
            if (creds[i].CredId == credId) {
                creds[i] = newCred;
                Q_EMIT dataChanged(index(i, 0), index(i, CC_ColumnCount - 1));
                break;
            }
        }
    }

    void remove(const QList<QString>& credIds) {
        if (credIds.isEmpty() || creds.isEmpty())
            return;

        QList<int> rowsToRemove;
        for (int i = 0; i < creds.size(); ++i) {
            if (credIds.contains(creds[i].CredId))
                rowsToRemove.append(i);
        }

        if (rowsToRemove.isEmpty())
            return;

        std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

        for (int row : rowsToRemove) {
            beginRemoveRows(QModelIndex(), row, row);
            creds.removeAt(row);
            endRemoveRows();
        }
    }

    void setTag(const QStringList &credIds, const QString &tag) {
        if (credIds.isEmpty() || creds.isEmpty())
            return;

        QSet<QString> idsSet(credIds.begin(), credIds.end());
        bool anyChanged = false;

        for (int i = 0; i < creds.size(); ++i) {
            if (idsSet.contains(creds[i].CredId)) {
                creds[i].Tag = tag;
                anyChanged = true;

                Q_EMIT dataChanged(index(i, CC_Tag), index(i, CC_Tag), {Qt::DisplayRole});

                idsSet.remove(creds[i].CredId);
                if (idsSet.isEmpty())
                    break;
            }
        }
    }

    void clear() {
        beginResetModel();
        creds.clear();
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
};

#endif