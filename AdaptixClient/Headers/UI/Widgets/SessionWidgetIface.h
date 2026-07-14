#ifndef ADAPTIXCLIENT_SESSIONWIDGETIFACE_H
#define ADAPTIXCLIENT_SESSIONWIDGETIFACE_H

#include <main.h>

namespace KDDockWidgets::QtWidgets { class DockWidget; }
class Agent;

class SessionWidgetIface
{
public:
    virtual ~SessionWidgetIface() = default;

    virtual KDDockWidgets::QtWidgets::DockWidget* dock() = 0;
    virtual void SetUpdatesEnabled(bool enabled) = 0;
    virtual void AddAgentItem(Agent* agent) = 0;
    virtual void UpdateAgentItem(const AgentData& oldData, const Agent* agent) = 0;
    virtual void RemoveAgentItem(qint64 agentId) = 0;
    virtual void UpdateData() = 0;
    virtual void UpdateAgentTypeComboBox() = 0;
    virtual void UpdateColumnsVisible() {}
    virtual void UpdateColumnsSize() {}
    virtual void UpdateLastColumn(const QList<qint64>& agentIds) { Q_UNUSED(agentIds); }
    virtual void Clear() = 0;
    virtual QWidget* asWidget() = 0;

    virtual void OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members) { Q_UNUSED(groupId); Q_UNUSED(parentId); Q_UNUSED(name); Q_UNUSED(members); }
    virtual void OnGroupRenamed(int64_t groupId, const QString& name) { Q_UNUSED(groupId); Q_UNUSED(name); }
    virtual void OnGroupDeleted(int64_t groupId) { Q_UNUSED(groupId); }
    virtual void OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove) { Q_UNUSED(groupId); Q_UNUSED(add); Q_UNUSED(remove); }
    virtual void OnGroupReparented(int64_t groupId, int64_t newParentId) { Q_UNUSED(groupId); Q_UNUSED(newParentId); }
};

#endif
