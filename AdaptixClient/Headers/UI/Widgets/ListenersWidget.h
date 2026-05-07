#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;

enum ListenersColumns {
    LC_Name,
    LC_RegName,
    LC_Type,
    LC_Protocol,
    LC_BindHost,
    LC_BindPort,
    LC_Hosts,
    LC_Date,
    LC_Status,
    LC_ColumnCount
};



class ListenersFilterProxyModel : public QSortFilterProxyModel
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
    explicit ListenersFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
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



class ListenersTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<ListenerData> listeners;
    QHash<QString, int>   nameToRow;

    void rebuildIndex() {
        nameToRow.clear();
        for (int i = 0; i < listeners.size(); ++i)
            nameToRow[listeners[i].Name] = i;
    }

public:
    explicit ListenersTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return listeners.size(); }
    int columnCount(const QModelIndex&) const override { return LC_ColumnCount; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= listeners.size())
            return {};

        const ListenerData& l = listeners.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case LC_Name:     return l.Name;
                case LC_RegName:  return l.ListenerRegName;
                case LC_Type:     return l.ListenerType;
                case LC_Protocol: return l.ListenerProtocol;
                case LC_BindHost: return l.BindHost;
                case LC_BindPort: return l.BindPort;
                case LC_Hosts:    return l.AgentAddresses;
                case LC_Date:     return l.Date;
                case LC_Status:   return l.Status;
                default: ;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case LC_Date: return l.DateTimestamp;
                default:      return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case LC_Type:
                case LC_Protocol:
                case LC_BindHost:
                case LC_BindPort:
                case LC_Date:
                case LC_Status:
                    return Qt::AlignCenter;
                case LC_Hosts:
                    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
                default: ;
            }
        }

        if (role == Qt::ForegroundRole) {
            if (index.column() == LC_Status) {
                if (l.Status == "Listen")
                    return QColor(COLOR_NeonGreen);
                else
                    return QColor(COLOR_ChiliPepper);
            }
        }

        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "Name", "Reg name", "Type", "Protocol", "Bind Host",
            "Bind Port", "C2 Hosts (agent)", "Date", "Status"
        };

        return headers.value(section);
    }

    void add(const ListenerData& item) {
        const int row = listeners.size();
        beginInsertRows(QModelIndex(), row, row);
        listeners.append(item);
        nameToRow[item.Name] = row;
        endInsertRows();
    }

    void update(const QString& name, const ListenerData& newListener) {
        auto it = nameToRow.find(name);
        if (it == nameToRow.end())
            return;

        int row = it.value();
        listeners[row].BindHost = newListener.BindHost;
        listeners[row].BindPort = newListener.BindPort;
        listeners[row].AgentAddresses = newListener.AgentAddresses;
        listeners[row].Status = newListener.Status;
        listeners[row].Data = newListener.Data;
        Q_EMIT dataChanged(index(row, 0), index(row, LC_ColumnCount - 1));
    }

    void remove(const QString& name) {
        auto it = nameToRow.find(name);
        if (it == nameToRow.end())
            return;

        int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        nameToRow.remove(listeners[row].Name);
        listeners.removeAt(row);
        endRemoveRows();

        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        listeners.clear();
        nameToRow.clear();
        endResetModel();
    }

    ListenerData getByName(const QString& name) const {
        auto it = nameToRow.find(name);
        if (it != nameToRow.end())
            return listeners.at(it.value());
        return {};
    }

    bool contains(const QString& name) const {
        return nameToRow.contains(name);
    }
};



class ListenersWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    ListenersTableModel*       listenersModel = nullptr;
    ListenersFilterProxyModel* proxyModel     = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    ClickableLabel* hideButton      = nullptr;

    void createUI();

public:
    explicit ListenersWidget(AdaptixWidget* w);
    ~ListenersWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void Clear() const;
    void AddListenerItem(const ListenerData &newListener) const;
    void EditListenerItem(const ListenerData &newListener) const;
    void RemoveListenerItem(const QString &listenerName) const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleListenersMenu( const QPoint &pos ) const;
    void onCreateListener() const;
    void onEditListener() const;
    void onRemoveListener() const;
    void onPauseListener() const;
    void onResumeListener() const;
    void onGenerateAgent() const;
};

#endif