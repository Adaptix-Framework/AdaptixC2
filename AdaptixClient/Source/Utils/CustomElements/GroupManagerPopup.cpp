#include <Utils/CustomElements/GroupManagerPopup.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Agent/Agent.h>
#include <Utils/Logs.h>

#include <oclero/qlementine/utils/ImageUtils.hpp>

#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QDropEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QShowEvent>
#include <QHideEvent>

class DropAwareTreeWidget : public QTreeWidget
{
    QList<QTreeWidgetItem*>                     dragSrc;
    std::function<void(qint64, qint64, qint64)> m_onAgentDrop;
    std::function<void(qint64, qint64, qint64)> m_onGroupDrop;

public:
    explicit DropAwareTreeWidget( std::function<void(qint64,qint64,qint64)> onAgentDrop, std::function<void(qint64,qint64,qint64)> onGroupDrop, QWidget* parent = nullptr) : QTreeWidget(parent), m_onAgentDrop(std::move(onAgentDrop)), m_onGroupDrop(std::move(onGroupDrop))
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        dragSrc = selectedItems();
        QTreeWidget::startDrag(supportedActions);
    }

    void dropEvent(QDropEvent* event) override {
        if (dragSrc.isEmpty()) { event->ignore(); return; }

        QTreeWidgetItem* target = itemAt(event->position().toPoint());

        if (!target) {
            bool moved = false;
            for (QTreeWidgetItem* src : std::as_const(dragSrc)) {
                if (!src->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool()) continue;
                qint64 groupId = src->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>();
                if (groupId == GroupManagerPopup::UNGROUPED_ID) continue;
                if (!src->parent()) continue; // already at root
                qint64 fromParentId = src->parent()->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>();
                if (m_onGroupDrop) { m_onGroupDrop(groupId, fromParentId, 0); moved = true; }
            }
            moved ? event->acceptProposedAction() : event->ignore();
            return;
        }

        QTreeWidgetItem* tgtGroup = nullptr;
        if (target->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool()) {
            tgtGroup = target;
        } else if (target->parent() && target->parent()->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool()) {
            tgtGroup = target->parent();
        }

        if (!tgtGroup) {
            event->ignore();
            return;
        }

        qint64 toGroupId = tgtGroup->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>();

        bool moved = false;
        for (QTreeWidgetItem* src : std::as_const(dragSrc)) {
            if (src == tgtGroup)
                continue;

            if (src->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool()) {
                qint64 groupId = src->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>();
                if (groupId == GroupManagerPopup::UNGROUPED_ID || groupId == toGroupId)
                    continue;
                QTreeWidgetItem* srcParent = src->parent();
                qint64 fromParentId = (srcParent && srcParent->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool())
                                      ? srcParent->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>()
                                      : 0;
                if (fromParentId != toGroupId && m_onGroupDrop) {
                    m_onGroupDrop(groupId, fromParentId, toGroupId);
                    moved = true;
                }
            } else {
                if (!m_onAgentDrop)
                    continue;
                qint64 agentId = src->data(0, GroupManagerPopup::ROLE_AGENT_ID).value<qint64>();
                QTreeWidgetItem* srcGrp = src->parent();
                qint64 fromGroupId = (srcGrp && srcGrp->data(0, GroupManagerPopup::ROLE_IS_GROUP).toBool())
                                     ? srcGrp->data(0, GroupManagerPopup::ROLE_GROUP_ID).value<qint64>()
                                     : GroupManagerPopup::UNGROUPED_ID;
                if (fromGroupId != toGroupId) {
                    m_onAgentDrop(agentId, fromGroupId, toGroupId);
                    moved = true;
                }
            }
        }
        moved ? event->acceptProposedAction() : event->ignore();
    }
};

GroupManagerPopup::GroupManagerPopup(GroupingProxyModel* model, AdaptixWidget* w, const QString& scope, QWidget* anchor, QWidget* parent) : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint), groupModel(model), aw(w), scope(scope), anchorWidget(anchor)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    createUI();
}

void GroupManagerPopup::createUI()
{
    tree = new DropAwareTreeWidget(
        [this](qint64 agentId, qint64 from, qint64 to) {
            doMoveAgent(agentId, from, to);
        },
        [this](qint64 groupId, qint64 fromParent, qint64 toParent) {
            doReparentGroup(groupId, fromParent, toParent);
        },
        this
    );
    tree->setHeaderHidden(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->setRootIsDecorated(true);
    tree->setProperty("autoIconColor", QVariant::fromValue(oclero::qlementine::AutoIconColor::None));
    tree->setAnimated(true);
    tree->setIndentation(25);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree);

    connect(tree, &QTreeWidget::customContextMenuRequested, this, &GroupManagerPopup::onContextMenu);
}

bool GroupManagerPopup::isGroupItem(QTreeWidgetItem* item) const
{
    return item && item->data(0, ROLE_IS_GROUP).toBool();
}

qint64 GroupManagerPopup::groupIdOf(QTreeWidgetItem* item) const
{
    return item ? item->data(0, ROLE_GROUP_ID).value<qint64>() : UNGROUPED_ID;
}

qint64 GroupManagerPopup::agentIdOf(QTreeWidgetItem* item) const
{
    return item ? item->data(0, ROLE_AGENT_ID).value<qint64>() : 0;
}

qint64 GroupManagerPopup::currentGroupId() const
{
    auto selected = tree->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        if (isGroupItem(item))
            return groupIdOf(item);
    }
    return UNGROUPED_ID;
}

void GroupManagerPopup::showPopup()
{
    Rebuild();

    const int popupWidth  = 480;
    const int popupHeight = 420;

    QPoint pos = anchorWidget->mapToGlobal(QPoint(0, anchorWidget->height()));
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (screen) {
        QRect sr = screen->availableGeometry();
        if (pos.x() + popupWidth > sr.right())
            pos.setX(sr.right() - popupWidth);
        if (pos.y() + popupHeight > sr.bottom())
            pos.setY(anchorWidget->mapToGlobal(QPoint(0, 0)).y() - popupHeight);
    }

    setGeometry(pos.x(), pos.y(), popupWidth, popupHeight);
    show();
    raise();
    activateWindow();
}

void GroupManagerPopup::captureExpansionState()
{
    std::function<void(QTreeWidgetItem*)> walkItem = [&](QTreeWidgetItem* item) {
        if (!item)
            return;
        if (item->data(0, ROLE_IS_GROUP).toBool()) {
            qint64 gid = item->data(0, ROLE_GROUP_ID).value<qint64>();
            if (item->isExpanded())
                collapsedGroupIds.remove(gid);
            else
                collapsedGroupIds.insert(gid);
        }
        for (int i = 0; i < item->childCount(); ++i)
            walkItem(item->child(i));
    };

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        walkItem(tree->topLevelItem(i));
}

bool GroupManagerPopup::shouldExpandGroup(qint64 groupId) const
{
    return !collapsedGroupIds.contains(groupId);
}

void GroupManagerPopup::Rebuild()
{
    captureExpansionState();
    tree->clear();

    if (!groupModel || !aw)
        return;

    QVector<GroupNode> groups = groupModel->allCustomGroups();
    QSet<qint64> assigned;
    for (const GroupNode& g : groups)
        for (qint64 id : g.memberIds)
            assigned.insert(id);

    auto agentIcon = [&](qint64 agentId) -> QIcon {
        QReadLocker lk(&aw->AgentsMapLock);
        Agent* a = aw->AgentsMap.value(agentId, nullptr);
        if (!a)
            return {};
        bool inactive = (a->data.Mark == "Inactive" || a->data.Mark == "Terminated" || a->data.Mark == "Disconnect");
        switch (a->data.Os) {
            case OS_WINDOWS:
                if (a->data.Elevated) return QIcon(":/icons/os_win_red");
                if (inactive)         return QIcon(":/icons/os_win_grey");
                return QIcon(":/icons/os_win_blue");
            case OS_LINUX:
                if (a->data.Elevated) return QIcon(":/icons/os_linux_red");
                if (inactive)         return QIcon(":/icons/os_linux_grey");
                return QIcon(":/icons/os_linux_blue");
            case OS_MAC:
                if (a->data.Elevated) return QIcon(":/icons/os_mac_red");
                if (inactive)         return QIcon(":/icons/os_mac_grey");
                return QIcon(":/icons/os_mac_blue");
            default:
                return {};
        }
    };

    auto agentLabel = [&](qint64 agentId) -> QString {
        QReadLocker lk(&aw->AgentsMapLock);
        Agent* a = aw->AgentsMap.value(agentId, nullptr);
        if (!a)
            return QString("#%1").arg(agentId);
        QString label = QString("#%1").arg(agentId);
        QString userHost = formatAgentUserHost(a->data);
        if (!userHost.isEmpty())
            label += "  " + userHost;
        else if (!a->data.ExternalIP.isEmpty())
            label += "  " + a->data.ExternalIP;
        if (!a->data.Pid.isEmpty())
            label += QString(" (%1)").arg(a->data.Pid);
        return label;
    };

    QIcon folderIcon = this->style()->standardIcon(QStyle::SP_DirIcon);

    QHash<int64_t, QTreeWidgetItem*> itemMap;

    for (const GroupNode& g : groups) {
        auto* groupItem = new QTreeWidgetItem();

        groupItem->setText(0, QString("%1  (%2)").arg(g.name).arg(g.memberIds.size()));
        groupItem->setIcon(0, folderIcon);
        groupItem->setData(0, ROLE_IS_GROUP, true);
        groupItem->setData(0, ROLE_GROUP_ID, QVariant::fromValue(g.groupId));
        QFont f = groupItem->font(0);
        f.setBold(true);
        groupItem->setFont(0, f);

        if (scope == "agents") {
            for (qint64 memberId : g.memberIds) {
                auto* agentItem = new QTreeWidgetItem(groupItem);
                agentItem->setText(0, agentLabel(memberId));
                agentItem->setIcon(0, agentIcon(memberId));
                agentItem->setData(0, ROLE_IS_GROUP, false);
                agentItem->setData(0, ROLE_AGENT_ID, QVariant::fromValue(memberId));
                agentItem->setData(0, ROLE_GROUP_ID, QVariant::fromValue(g.groupId));
            }
        }
        itemMap[g.groupId] = groupItem;
    }

    for (const GroupNode& g : groups) {
        QTreeWidgetItem* item = itemMap[g.groupId];
        if (g.parentGroupId != 0 && itemMap.contains(g.parentGroupId)) {
            itemMap[g.parentGroupId]->addChild(item);
        } else {
            tree->addTopLevelItem(item);
        }
    }

    for (const GroupNode& g : groups) {
        if (QTreeWidgetItem* item = itemMap.value(g.groupId)) {
            item->setExpanded(shouldExpandGroup(g.groupId));
        }
    }

    if (scope == "agents") {
        auto* ungrouped = new QTreeWidgetItem(tree);
        QList<qint64> ungroupedIds;
        {
            QReadLocker lk(&aw->AgentsMapLock);
            for (auto it = aw->AgentsMap.begin(); it != aw->AgentsMap.end(); ++it) {
                if (!assigned.contains(it.key()))
                    ungroupedIds.append(it.key());
            }
        }
        for (qint64 id : ungroupedIds) {
            auto* agentItem = new QTreeWidgetItem(ungrouped);
            agentItem->setIcon(0, agentIcon(id));
            agentItem->setData(0, ROLE_IS_GROUP, false);
            agentItem->setData(0, ROLE_AGENT_ID, QVariant::fromValue(id));
            agentItem->setData(0, ROLE_GROUP_ID, QVariant::fromValue(UNGROUPED_ID));
            agentItem->setText(0, agentLabel(id));
        }
        ungrouped->setText(0, QString("Ungrouped  (%1)").arg(ungroupedIds.size()));
        ungrouped->setIcon(0, folderIcon);
        ungrouped->setData(0, ROLE_IS_GROUP, true);
        ungrouped->setData(0, ROLE_GROUP_ID, QVariant::fromValue(UNGROUPED_ID));
        QFont f2 = ungrouped->font(0);
        f2.setBold(true);
        ungrouped->setFont(0, f2);
        ungrouped->setExpanded(shouldExpandGroup(UNGROUPED_ID));
    }
}

void GroupManagerPopup::doMoveAgent(qint64 agentId, qint64 fromGroupId, qint64 toGroupId)
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    const int64_t from = (fromGroupId == UNGROUPED_ID || fromGroupId < 0) ? 0 : static_cast<int64_t>(fromGroupId);
    const int64_t to   = (toGroupId == UNGROUPED_ID || toGroupId < 0) ? 0 : static_cast<int64_t>(toGroupId);
    if (from == to && to == 0)
        return;

    HttpReqGroupMoveMemberAsync(agentId, from, to, *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to move agent between groups") : message);
    });
}

void GroupManagerPopup::doReparentGroup(qint64 groupId, qint64 fromParentId, qint64 toGroupId)
{
    Q_UNUSED(fromParentId)
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;
    const int64_t parent = (toGroupId == UNGROUPED_ID || toGroupId < 0) ? 0 : static_cast<int64_t>(toGroupId);
    HttpReqGroupReparentAsync(groupId, parent, *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to reparent group") : message);
    });
}

void GroupManagerPopup::onNewGroup()
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    bool ok = false;
    QString name = QInputDialog::getText(aw, "New Group", "Group name:", QLineEdit::Normal, QString(), &ok);
    showPopup();
    if (!ok || name.trimmed().isEmpty())
        return;

    HttpReqGroupCreateAsync(0, name.trimmed(), scope, *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to create group") : message);
    });
}

void GroupManagerPopup::onNewSubgroup()
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    qint64 parentGroupId = currentGroupId();
    if (parentGroupId == UNGROUPED_ID)
        return;

    bool ok = false;
    QString name = QInputDialog::getText(aw, "New Subgroup", "Subgroup name:", QLineEdit::Normal, QString(), &ok);
    showPopup();
    if (!ok || name.trimmed().isEmpty())
        return;

    HttpReqGroupCreateAsync(parentGroupId, name.trimmed(), scope, *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to create subgroup") : message);
    });
}

void GroupManagerPopup::onRenameGroup()
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    qint64 groupId = currentGroupId();
    if (groupId == UNGROUPED_ID)
        return;

    QString currentName;
    for (const GroupNode& g : groupModel->allCustomGroups()) {
        if (g.groupId == groupId) {
            currentName = g.name;
            break;
        }
    }

    bool ok = false;
    QString name = QInputDialog::getText(aw, "Rename Group", "New name:", QLineEdit::Normal, currentName, &ok);
    showPopup();
    if (!ok || name.trimmed().isEmpty())
        return;

    HttpReqGroupRenameAsync(groupId, name.trimmed(), *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to rename group") : message);
    });
}

void GroupManagerPopup::onDeleteGroup()
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    qint64 groupId = currentGroupId();
    if (groupId == UNGROUPED_ID)
        return;

    QString groupName;
    for (const GroupNode& g : groupModel->allCustomGroups()) {
        if (g.groupId == groupId) {
            groupName = g.name;
            break;
        }
    }

    auto btn = QMessageBox::question(aw, "Delete Group",
        QString("Delete group \"%1\"?\nAgents become ungrouped; subgroups move one level up.").arg(groupName),
        QMessageBox::Yes | QMessageBox::No);
    showPopup();
    if (btn != QMessageBox::Yes)
        return;

    HttpReqGroupDeleteAsync(groupId, *profile, [](bool ok, const QString& message, const QJsonObject&) {
        if (!ok)
            MessageError(message.isEmpty() ? QStringLiteral("Failed to delete group") : message);
    });
}

void GroupManagerPopup::onContextMenu(const QPoint& pos)
{
    AuthProfile* profile = aw->GetProfile();
    if (!profile)
        return;

    QTreeWidgetItem* item = tree->itemAt(pos);

    QMenu menu(this);
    QAction* actNew    = menu.addAction("New group");
    QAction* actNewSub = nullptr;
    QAction* actRename = nullptr;
    QAction* actDelete = nullptr;
    QAction* actRemove = nullptr;
    QAction* actMoveToRoot  = nullptr;
    QHash<QAction*, qint64> moveActions;

    qint64 agentId = 0;
    qint64 curGroup = UNGROUPED_ID;

    if (item && isGroupItem(item)) {
        qint64 groupId = groupIdOf(item);
        if (groupId != UNGROUPED_ID) {
            actRename = menu.addAction("Rename");
            actNewSub = menu.addAction("New Subgroup");
            if (item->parent())  // is a subgroup → can be promoted to root
                actMoveToRoot = menu.addAction("Move to root");
            actDelete = menu.addAction("Delete");
        }
    } else if (item) {
        agentId = agentIdOf(item);
        curGroup = item->parent()
            ? groupIdOf(item->parent())
            : UNGROUPED_ID;

        menu.addSeparator();
        auto* moveSub = menu.addMenu("Move to group");
        for (const GroupNode& g : groupModel->allCustomGroups()) {
            if (g.groupId == curGroup)
                continue;

            QAction* act = moveSub->addAction(g.name);
            moveActions[act] = g.groupId;
        }

        if (curGroup != UNGROUPED_ID) {
            menu.addSeparator();
            actRemove = menu.addAction("Remove from group");
        }
    }

    QAction* chosen = nullptr;
    QObject::connect(&menu, &QMenu::triggered, this, [&chosen](QAction* a) { chosen = a; });
    menu.exec(QCursor::pos());
    if (!chosen)
        return;

    if (chosen == actNew) {
        onNewGroup();
    } else if (chosen == actNewSub) {
        onNewSubgroup();
    } else if (chosen == actRename) {
        onRenameGroup();
    } else if (chosen == actDelete) {
        onDeleteGroup();
    } else if (chosen == actMoveToRoot) {
        qint64 groupId   = groupIdOf(item);
        qint64 fromParent = item->parent() ? groupIdOf(item->parent()) : 0;
        doReparentGroup(groupId, fromParent, 0);
        showPopup();
    } else if (chosen == actRemove) {
        doMoveAgent(agentId, curGroup, UNGROUPED_ID);
        showPopup();
    } else if (moveActions.contains(chosen)) {
        doMoveAgent(agentId, curGroup, moveActions[chosen]);
        showPopup();
    }
}
