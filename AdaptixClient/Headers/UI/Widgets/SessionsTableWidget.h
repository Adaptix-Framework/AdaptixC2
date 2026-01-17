#ifndef ADAPTIXCLIENT_SESSIONSTABLEWIDGET_H
#define ADAPTIXCLIENT_SESSIONSTABLEWIDGET_H

#include <main.h>
#include <MainAdaptix.h>
#include <Utils/CustomElements.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>
#include <Client/Settings.h>

#include <QSortFilterProxyModel>

class Agent;
class AdaptixWidget;

enum SessionsColumns {
    SC_AgentID,
    SC_AgentType,
    SC_External,
    SC_Listener,
    SC_Internal,
    SC_Domain,
    SC_Computer,
    SC_User,
    SC_Os,
    SC_Process,
    SC_Pid,
    SC_Tid,
    SC_Tags,
    SC_Created,
    SC_Last,
    SC_Sleep,
    SC_ColumnCount
};



class AgentsFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;
    bool    searchVisible  = false;
    bool    onlyActive     = false;
    QString textFilter;
    QSet<QString> agentTypes;

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
    explicit AgentsFilterProxyModel(AdaptixWidget* adaptix, QObject* parent = nullptr) : QSortFilterProxyModel(parent), adaptixWidget(adaptix) {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortRole(Qt::UserRole);
    }

    void setSearchVisible(bool visible) {
        if (searchVisible == visible) return;
        searchVisible = visible;
        invalidateFilter();
    }
    void setOnlyActive(bool active) {
        if (onlyActive == active) return;
        onlyActive = active;
        invalidateFilter();
    }
    void setTextFilter(const QString& text) {
        if (textFilter == text) return;
        textFilter = text;
        invalidateFilter();
    }
    void setAgentTypes(const QSet<QString>& types) {
        if (agentTypes == types) return;
        agentTypes = types;
        invalidateFilter();
    }
    void updateVisible() {
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        if (!adaptixWidget || !sourceModel())
            return true;

        QModelIndex idxId = sourceModel()->index(row, 0, parent);
        if (!idxId.isValid())
            return true;

        QString agentId = sourceModel()->data(idxId, Qt::DisplayRole).toString();
        if (agentId.isEmpty())
            return true;

        const Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (!agent || !agent->show)
            return false;

        const AgentData &a = agent->data;

        if (onlyActive) {
            if (a.Mark == "Terminated" || a.Mark == "Inactive" || a.Mark == "Disconnect")
                return false;
        }

        if (!agentTypes.isEmpty() && !agentTypes.contains(a.Name))
            return false;

        QString username = a.Username;
        if (a.Elevated)
            username = "* " + username;
        if (!a.Impersonated.isEmpty())
            username += " [" + a.Impersonated + "]";

        if (!textFilter.isEmpty()) {
            QString rowData = a.Id + " " + a.Name + " " + a.Listener + " " + a.ExternalIP + " " +
                              a.InternalIP + " " + a.Process + " " + a.OsDesc + " " + a.Domain + " " +
                              a.Computer + " " + username + " " + a.Tags;
            if (!evaluateExpression(textFilter, rowData))
                return false;
        }

        return true;
    }
};



class AgentsTableModel : public QAbstractTableModel
{
Q_OBJECT
    AdaptixWidget*        adaptixWidget;
    QVector<QString>      agentsId;
    QHash<QString, int>   idToRow;

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

    QVariant data(const QModelIndex &index, const int role) const override {
        if (!index.isValid())
            return {};

        QString agentId = agentsId.at(index.row());
        Agent*  agent   = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (!agent)
            return {};

        AgentData d = agent->data;

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case SC_AgentID:   return d.Id;
                case SC_AgentType: return d.Name;
                case SC_External:  return d.ExternalIP;
                case SC_Listener:  return d.Listener;
                case SC_Internal:  return d.InternalIP;
                case SC_Domain:    return d.Domain;
                case SC_Computer:  return d.Computer;
                case SC_User:
                {
                    QString username = d.Username;
                    if ( d.Elevated ) username = "* " + username;
                    if ( d.Impersonated != "" ) username += " [" + d.Impersonated + "]";
                    return username;
                }
                case SC_Os:        return d.OsDesc;
                case SC_Process:
                {
                    QString process = d.Process;
                    if ( !d.Arch.isEmpty() )
                        process += QString(" (%2)").arg(d.Arch);
                    return process;
                }
                case SC_Pid:       return d.Pid;
                case SC_Tid:       return d.Tid;
                case SC_Tags:      return d.Tags;
                case SC_Created:   return d.Date;
                case SC_Last:
                {
                    if ( d.Mark.isEmpty() || d.Mark == "No response" || d.Mark == "No worktime" ) {
                        return agent->LastMark;
                    }
                    return UnixTimestampGlobalToStringLocalSmall(d.LastTick);
                }
                case SC_Sleep:
                {
                    if ( d.Mark.isEmpty() ) {
                        if ( !d.Async ) {
                            if ( agent->connType == "internal" )
                                return QString::fromUtf8("\u221E  \u221E");
                            else
                                return QString::fromUtf8("\u27F6\u27F6\u27F6");
                        }
                        return QString("%1 (%2%)").arg( FormatSecToStr(d.Sleep) ).arg(d.Jitter);
                    }
                    return d.Mark;
                }
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case SC_Last:    return d.LastTick;
                case SC_Created: return d.DateTimestamp;
                default:         return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case SC_AgentID:
                case SC_AgentType:
                case SC_External:
                case SC_Internal:
                case SC_Listener:
                case SC_Domain:
                case SC_Computer:
                case SC_User:
                case SC_Os:
                // case SC_Process:
                case SC_Pid:
                case SC_Tid:
                case SC_Created:
                case SC_Last:
                case SC_Sleep:
                    return Qt::AlignCenter;
            }
        }

        if (role == Qt::DecorationRole) {
            if (index.column() == SC_Os) {
                return agent->iconOs;
            }
        }

        if (role == Qt::BackgroundRole) {
            if (agent->bg_color.isValid())
                return agent->bg_color;
            return QVariant();
        }
        if (role == Qt::ForegroundRole) {
            if (agent->fg_color.isValid())
                return agent->fg_color;
            return QVariant();
        }

        if (role == Qt::ToolTipRole) {
            if (index.column() == SC_Sleep) {
                QString WorkAndKill = "";
                if (d.WorkingTime || d.KillDate) {
                    if (d.WorkingTime) {
                        uint startH = ( d.WorkingTime >> 24 ) % 64;
                        uint startM = ( d.WorkingTime >> 16 ) % 64;
                        uint endH   = ( d.WorkingTime >>  8 ) % 64;
                        uint endM   = ( d.WorkingTime >>  0 ) % 64;

                        QChar c = QLatin1Char('0');
                        WorkAndKill = QString("Work time: %1:%2 - %3:%4\n").arg(startH, 2, 10, c).arg(startM, 2, 10, c).arg(endH, 2, 10, c).arg(endM, 2, 10, c);
                    }
                    if (d.KillDate) {
                        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(d.KillDate);
                        WorkAndKill += QString("Kill date: %1").arg(dateTime.toString("dd.MM.yyyy hh:mm:ss"));
                    }
                }
                return WorkAndKill;
            }
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "Agent Id", "Type", "External", "Listener", "Internal",
            "Domain", "Computer", "User", "OS", "Process",
            "PID", "TID", "Tags", "Created", "Last", "Sleep"
        };

        return headers.value(section);
    }

    void add(const QString &agentId) {
        const int row = agentsId.size();

        beginInsertRows(QModelIndex(), row, row);
        agentsId.append(agentId);
        idToRow[agentId] = row;
        endInsertRows();
    }

    void update(const QString &agentId) {
        auto it = idToRow.find(agentId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        Q_EMIT dataChanged(index(row, 0), index(row, SC_ColumnCount - 1), { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole });
    }

    void updateLastColumn(const QStringList &agentIds) {
        if (agentIds.isEmpty())
            return;

        int minRow = INT_MAX;
        int maxRow = -1;

        for (const QString &agentId : agentIds) {
            auto it = idToRow.find(agentId);
            if (it == idToRow.end())
                continue;

            int row = it.value();
            if (row < minRow) minRow = row;
            if (row > maxRow) maxRow = row;
        }

        if (maxRow >= 0) {
            Q_EMIT dataChanged(index(minRow, 0), index(maxRow, SC_ColumnCount - 1), { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole });
        }
    }

    void remove(const QString &agentId) {
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
};



class SessionsTableWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;

    QGridLayout*  mainGridLayout = nullptr;
    QTableView*   tableView      = nullptr;
    QMenu*        menuSessions   = nullptr;
    QShortcut*    shortcutSearch = nullptr;

    AgentsFilterProxyModel* proxyModel  = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    QComboBox*      comboAgentType  = nullptr;
    QCheckBox*      checkOnlyActive = nullptr;
    ClickableLabel* hideButton      = nullptr;

    void createUI();

public:
    AgentsTableModel* agentsModel = nullptr;
    explicit SessionsTableWidget( AdaptixWidget* w );
    ~SessionsTableWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddAgentItem(Agent* newAgent) const;
    void UpdateAgentItem(const AgentData &oldDatam, const Agent* agent) const;
    void RemoveAgentItem(const QString &agentId) const;

    void UpdateColumnsVisible() const;
    void UpdateColumnsSize() const;
    void UpdateData() const;
    void UpdateAgentTypeComboBox() const;
    void Clear() const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterChanged() const;
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
    void actionConsoleDelete();
    void actionItemTag() const;
    void actionItemHide() const;
    void actionItemsShowAll() const;
    void actionSetData() const;
};

#endif
