#ifndef ADAPTIXCLIENT_AGENTSFILTERPROXYMODEL_H
#define ADAPTIXCLIENT_AGENTSFILTERPROXYMODEL_H

#include <main.h>
#include <Utils/FilterExpression.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>

#include <QSortFilterProxyModel>

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
    SC_Icon,
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
    FilterExpression filterAst;
    QSet<QString> agentTypes;

public:
    explicit AgentsFilterProxyModel(AdaptixWidget* adaptix, QObject* parent = nullptr) : QSortFilterProxyModel(parent), adaptixWidget(adaptix)
    {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortRole(Qt::UserRole);
    }

    void setSearchVisible(bool visible) {
        if (searchVisible == visible)
            return;
        searchVisible = visible;
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
        invalidateFilter();
QT_WARNING_POP
    }
    void setOnlyActive(bool active) {
        if (onlyActive == active)
            return;
        onlyActive = active;
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
        invalidateFilter();
QT_WARNING_POP
    }
    void setTextFilter(const QString& text) {
        if (textFilter == text)
            return;
        textFilter = text;
        filterAst.compile(text);
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
        invalidateFilter();
QT_WARNING_POP
    }
    void setAgentTypes(const QSet<QString>& types) {
        if (agentTypes == types)
            return;
        agentTypes = types;
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
        invalidateFilter();
QT_WARNING_POP
    }
    void updateVisible() {
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
        invalidateFilter();
QT_WARNING_POP
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        if (!adaptixWidget || !sourceModel())
            return true;

        QModelIndex idxId = sourceModel()->index(row, 0, parent);
        if (!idxId.isValid())
            return true;

        qint64 agentId = sourceModel()->data(idxId, Qt::UserRole).toLongLong();
        if (agentId == 0)
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

        if (!filterAst.isEmpty()) {
            QString rowData = QString::number(a.Id) + " " + a.Name + " " + a.Listener + " " + a.ExternalIP + " " +
                              a.InternalIP + " " + a.Process + " " + a.OsDesc + " " + a.Domain + " " +
                              a.Computer + " " + username + " " + a.Tags;
            if (!filterAst.evaluate(rowData))
                return false;
        }
        return true;
    }
};

#endif
