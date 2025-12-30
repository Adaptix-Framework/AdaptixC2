#ifndef TUNNELSWIDGET_H
#define TUNNELSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;

enum TunnelsColumns {
    TUC_TunnelId,
    TUC_AgentId,
    TUC_Computer,
    TUC_User,
    TUC_Process,
    TUC_Type,
    TUC_Info,
    TUC_Interface,
    TUC_Port,
    TUC_Client,
    TUC_Fhost,
    TUC_Fport,
    TUC_ColumnCount
};



class TunnelsFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    QString textFilter;

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
    explicit TunnelsFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setSortRole(Qt::UserRole);
    }

    void setTextFilter(const QString &text) {
        if (textFilter == text) return;
        textFilter = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        auto model = sourceModel();
        if (!model)
            return true;

        if (!textFilter.isEmpty()) {
            QString rowData;
            for (int col = 0; col < model->columnCount(); ++col) {
                rowData += model->index(row, col, parent).data().toString() + " ";
            }
            if (!evaluateExpression(textFilter, rowData))
                return false;
        }

        return true;
    }
};



class TunnelsTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<TunnelData> tunnels;
    QHash<QString, int> idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < tunnels.size(); ++i)
            idToRow[tunnels[i].TunnelId] = i;
    }

public:
    explicit TunnelsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return tunnels.size(); }
    int columnCount(const QModelIndex&) const override { return TUC_ColumnCount; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= tunnels.size())
            return {};

        const TunnelData& t = tunnels.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case TUC_TunnelId:  return t.TunnelId;
                case TUC_AgentId:   return t.AgentId;
                case TUC_Computer:  return t.Computer;
                case TUC_User:      return t.Username;
                case TUC_Process:   return t.Process;
                case TUC_Type:      return t.Type;
                case TUC_Info:      return t.Info;
                case TUC_Interface: return t.Interface;
                case TUC_Port:      return t.Port;
                case TUC_Client:    return t.Client;
                case TUC_Fhost:     return t.Fhost;
                case TUC_Fport:     return t.Fport;
            }
        }

        if (role == Qt::UserRole) {
            return data(index, Qt::DisplayRole);
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case TUC_TunnelId:
                case TUC_AgentId:
                case TUC_Computer:
                case TUC_User:
                case TUC_Process:
                case TUC_Interface:
                case TUC_Port:
                case TUC_Client:
                case TUC_Fhost:
                case TUC_Fport:
                    return Qt::AlignCenter;
                case TUC_Type:
                case TUC_Info:
                    return int(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }

        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "Tunnel ID", "Agent ID", "Computer", "User", "Process", "Type",
            "Info", "Interface", "Listen port", "Client", "Target host", "Target port"
        };

        return headers.value(section);
    }

    void add(const TunnelData& item) {
        const int row = tunnels.size();
        beginInsertRows(QModelIndex(), row, row);
        tunnels.append(item);
        idToRow[item.TunnelId] = row;
        endInsertRows();
    }

    void updateInfo(const QString& tunnelId, const QString& info) {
        auto it = idToRow.find(tunnelId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        tunnels[row].Info = info;
        Q_EMIT dataChanged(index(row, TUC_Info), index(row, TUC_Info));
    }

    void remove(const QString& tunnelId) {
        auto it = idToRow.find(tunnelId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        idToRow.remove(tunnels[row].TunnelId);
        tunnels.removeAt(row);
        endRemoveRows();

        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        tunnels.clear();
        idToRow.clear();
        endResetModel();
    }

    bool contains(const QString& tunnelId) const {
        return idToRow.contains(tunnelId);
    }

    TunnelData getById(const QString& tunnelId) const {
        auto it = idToRow.find(tunnelId);
        if (it != idToRow.end())
            return tunnels.at(it.value());
        return {};
    }
};



class TunnelsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    TunnelsTableModel*       tunnelsModel = nullptr;
    TunnelsFilterProxyModel* proxyModel   = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    ClickableLabel* hideButton      = nullptr;

    void createUI();

public:
    explicit TunnelsWidget(AdaptixWidget* w);
    ~TunnelsWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void Clear() const;
    void AddTunnelItem(TunnelData newTunnel) const;
    void EditTunnelItem(const QString &tunnelId, const QString &info) const;
    void RemoveTunnelItem(const QString &tunnelId) const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleTunnelsMenu(const QPoint &pos) const;
    void actionSetInfo() const;
    void actionStopTunnel() const;
};

#endif
