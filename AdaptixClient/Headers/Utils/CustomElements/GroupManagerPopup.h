#ifndef ADAPTIXCLIENT_GROUPMANAGERPOPUP_H
#define ADAPTIXCLIENT_GROUPMANAGERPOPUP_H

#include <main.h>
#include <UI/Models/GroupingProxyModel.h>

class AdaptixWidget;

class GroupManagerPopup : public QFrame
{
Q_OBJECT
    GroupingProxyModel* groupModel   = nullptr;
    AdaptixWidget*      aw           = nullptr;
    QWidget*            anchorWidget = nullptr;
    QTreeWidget*        tree         = nullptr;
    QString             scope;

    QSet<qint64> collapsedGroupIds;

    void createUI();
    void captureExpansionState();
    bool shouldExpandGroup(qint64 groupId) const;

    bool   isGroupItem(QTreeWidgetItem* item) const;
    qint64 groupIdOf(QTreeWidgetItem* item)   const;
    qint64 agentIdOf(QTreeWidgetItem* item)   const;
    qint64 currentGroupId()                   const;

    void doMoveAgent(qint64 agentId, qint64 fromGroupId, qint64 toGroupId);
    void doReparentGroup(qint64 groupId, qint64 fromParentId, qint64 toGroupId);

public:
    static constexpr qint64 UNGROUPED_ID = -1LL;

    static const int ROLE_GROUP_ID = Qt::UserRole;
    static const int ROLE_AGENT_ID = Qt::UserRole + 1;
    static const int ROLE_IS_GROUP = Qt::UserRole + 2;

    explicit GroupManagerPopup(GroupingProxyModel* model, AdaptixWidget* w, const QString& scope, QWidget* anchor, QWidget* parent = nullptr);

    void showPopup();
    void Rebuild();

private Q_SLOTS:
    void onNewGroup();
    void onNewSubgroup();
    void onRenameGroup();
    void onDeleteGroup();
    void onContextMenu(const QPoint& pos);
};

#endif
