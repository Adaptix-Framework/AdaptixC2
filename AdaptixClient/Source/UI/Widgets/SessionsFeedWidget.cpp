#include <Agent/Agent.h>
#include <UI/Widgets/SessionsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Settings.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <UI/Dialogs/DialogAgentData.h>
#include <Utils/FontManager.h>
#include <Utils/Logs.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QPainter>
#include <QMouseEvent>

namespace SessionsBlock { enum { Icon = 0, Id = 1, Main = 2, Tags = 3, Right = 4, Count = 5 }; }

static bool sessionsCol(int col)
{
    return GlobalClient && GlobalClient->settings && GlobalClient->settings->data.SessionsTableColumns[col];
}

static FeedRow agentToFeedRow(const Agent* agent) {
    FeedRow row;
    row.resize(SessionsBlock::Count);
    if (!agent)
        return row;

    const AgentData& d = agent->data;
    row.entityId = d.Id;
    if (sessionsCol(SC_Icon))
        row[SessionsBlock::Icon] = agent->iconOs;

    QStringList tagList = d.Tags.split(",", Qt::SkipEmptyParts);
    const QString idStr = sessionsCol(SC_AgentID) ? QString("#%1").arg(d.Id) : QString();
    const QString badge = sessionsCol(SC_AgentType) ? d.Name : QString();
    const QString date  = sessionsCol(SC_Created)
        ? QDateTime::fromSecsSinceEpoch(d.DateTimestamp).toString("dd/MM HH:mm:ss")
        : QString();
    row[SessionsBlock::Id] = QVariantMap{
        {"id", idStr},
        {"idNum", d.Id},
        {"badge", badge},
        {"date", date},
        {"firstTag", tagList.isEmpty() ? QString() : tagList.first().toLower()}
    };

    QString user = d.Username;
    if (d.Elevated)
        user = "* " + user;
    if (!d.Impersonated.isEmpty())
        user += " [" + d.Impersonated + "]";

    QStringList mainParts;
    if (sessionsCol(SC_User) && !user.isEmpty())
        mainParts << user;
    if (sessionsCol(SC_Computer) && !d.Computer.isEmpty())
        mainParts << d.Computer;

    QString mainText = mainParts.join(QString(" \u00B7 "));
    if (sessionsCol(SC_Domain) && !d.Domain.isEmpty()) {
        if (!mainText.isEmpty())
            mainText += " @ " + d.Domain;
        else
            mainText = d.Domain;
    }

    const QString submain = sessionsCol(SC_Internal) ? d.InternalIP : QString();

    QStringList secondParts;
    if (sessionsCol(SC_Listener) || sessionsCol(SC_External)) {
        QString part;
        if (sessionsCol(SC_Listener) && !d.Listener.isEmpty())
            part = d.Listener;
        if (sessionsCol(SC_External) && !d.ExternalIP.isEmpty()) {
            if (!part.isEmpty())
                part += QString(" (%1)").arg(d.ExternalIP);
            else
                part = d.ExternalIP;
        }
        if (!part.isEmpty())
            secondParts << part;
    }
    if (sessionsCol(SC_Os) && !d.OsDesc.isEmpty())
        secondParts << d.OsDesc;

    if (sessionsCol(SC_Process) || sessionsCol(SC_Pid) || sessionsCol(SC_Tid)) {
        QString procPart;
        if (sessionsCol(SC_Process) && !d.Process.isEmpty()) {
            procPart = d.Process;
            QStringList extras;
            if (!d.Arch.isEmpty())
                extras << d.Arch;
            if (sessionsCol(SC_Pid) && !d.Pid.isEmpty())
                extras << d.Pid;
            if (sessionsCol(SC_Tid) && !d.Tid.isEmpty())
                extras << d.Tid;
            if (!extras.isEmpty())
                procPart += QString(" (%1)").arg(extras.join(", "));
        } else {
            QStringList ids;
            if (sessionsCol(SC_Pid) && !d.Pid.isEmpty())
                ids << d.Pid;
            if (sessionsCol(SC_Tid) && !d.Tid.isEmpty())
                ids << d.Tid;
            procPart = ids.join(", ");
        }
        if (!procPart.isEmpty())
            secondParts << procPart;
    }

    const QString secondText = secondParts.join(" | ");

    quint32 ipNum = 0;
    if (!d.InternalIP.isEmpty()) {
        QStringList octets = d.InternalIP.split('.');
        if (octets.size() == 4)
            ipNum = (octets[0].toUInt() << 24) | (octets[1].toUInt() << 16) | (octets[2].toUInt() << 8) | octets[3].toUInt();
    }

    row[SessionsBlock::Main] = QVariantMap{
        {"main", mainText},
        {"submain", submain},
        {"second", secondText},
        {"agentType", d.Name.toLower()},
        {"user", user.toLower()},
        {"computer", d.Computer.toLower()},
        {"domain", d.Domain.toLower()},
        {"listener", d.Listener.toLower()},
        {"process", d.Process.toLower()},
        {"ipNum", ipNum}
    };
    row[SessionsBlock::Tags] = sessionsCol(SC_Tags) ? tagList : QStringList();

    QString rightMain, rightSecond, rightStatus, rightStatusType;
    qint64 dateNum = d.LastTick;

    if (!d.Mark.isEmpty()) {
        if (sessionsCol(SC_Sleep)) {
            rightStatus = d.Mark;
            if (d.Mark == "Terminated") rightStatusType = "error";
            else if (d.Mark == "Inactive") rightStatusType = "error";
            else if (d.Mark == "Disconnect") rightStatusType = "error";
            else if (d.Mark == "No response") rightStatusType = "hosted";
            else if (d.Mark == "No worktime") rightStatusType = "hosted";
            else rightStatusType = "canceled";
        }

        if (sessionsCol(SC_Last)) {
            if (d.Mark == "No response")
                rightSecond = agent->LastMark;
            else
                rightSecond = UnixTimestampGlobalToStringLocalSmall(d.LastTick);
        }
    } else {
        if (!d.Async) {
            if (sessionsCol(SC_Sleep)) {
                if (agent->connType == "internal") {
                    if (agent->parentId != 0)
                        rightMain = QString::fromUtf8("\u221E\u221E\u221E");
                    else
                        rightMain = QString::fromUtf8("\u221E  \u221E");
                } else {
                    rightMain = QString::fromUtf8("\u27F6\u27F6\u27F6");
                }
            }
            rightStatusType = "running";
        } else {
            if (sessionsCol(SC_Last))
                rightMain = agent->LastMark;
            if (sessionsCol(SC_Sleep))
                rightSecond = QString("%1 (%2%)").arg(FormatSecToStr(d.Sleep)).arg(d.Jitter);
            rightStatusType = "running";
        }
    }

    row[SessionsBlock::Right] = QVariantMap{{"main", rightMain}, {"second", rightSecond}, {"status", rightStatus}, {"statusType", rightStatusType}, {"dateNum", dateNum}};
    row.isDead = !d.Mark.isEmpty() && (d.Mark == "Terminated" || d.Mark == "Inactive" || d.Mark == "Disconnect" || d.Mark == "No response" || d.Mark == "No worktime");
    if (agent->bg_color.isValid())
        row.backgroundColor = agent->bg_color;
    return row;
}

static ListFeedDelegate* createSessionsDelegate(QObject* parent) {
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IconBlock());
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    d->addBlock(new GroupHeaderBlock());
    return d;
}



SessionsFeedWidget::SessionsFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createSessionsDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    filterModel = new AgentsFilterProxyModel(w, this);
    filterModel->setSourceModel(feedBlockModel);
    filterModel->setSortRole(Qt::UserRole);
    filterModel->setDynamicSortFilter(true);

    setModel(feedBlockModel);
    setFilterModel(filterModel);
    setDelegate(delegate);

    m_groupingAdaptixWidget = w;
    setGroupingScope(QStringLiteral("agents"));
    rebuildModelChain();

    enableSearch(true);
    enableAutoCheck(true);
    enableFilterCombo(true, "All types");
    enableSortingCombo(true, {"No sorting", "Date", "Tag", "Status", "Agent ID", "Agent Type", "Listener", "User", "Computer", "Domain", "Internal IP", "Process"});
    enableActiveFilter(true);
    if (activeFilter() && GlobalClient && GlobalClient->settings)
        activeFilter()->setChecked(GlobalClient->settings->data.SessionsAutoHideInactive);
    enableGroupCombo(true);
    if (groupCombo()) {
        groupCombo()->blockSignals(true);
        groupCombo()->clear();
        groupCombo()->addItem("No grouping");
        groupCombo()->addItem("By Domain");
        groupCombo()->addItem("By Computer");
        groupCombo()->addItem("By User");
        groupCombo()->addItem("By Tag");
        groupCombo()->addItem("By Listener");
        groupCombo()->addItem("By OS");
        groupCombo()->addItem("By Agent Type");
        groupCombo()->addItem("Custom Groups");
        groupCombo()->addItem("Pivot Tree");
        groupCombo()->blockSignals(false);
    }
    finalizeSearchWidget();

    enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        setCompactMode(GlobalClient->settings->data.SessionsCompactMode);
    setBlockGap(12);

    setupCustomGroupsUi();
    wireGroupingProxy(groupingProxy());

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "SessionsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Sessions");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/format_list"), KDDockWidgets::IconPlace::TabBar);

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &SessionsFeedWidget::handleFeedMenu);
    connect(treeView(), &QTreeView::doubleClicked, this, &SessionsFeedWidget::onItemDoubleClicked);
}

SessionsFeedWidget::~SessionsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* SessionsFeedWidget::dock() { return this->dockWidget; }

void SessionsFeedWidget::rebuildAgentIndex()
{
    agentIdToRow.clear();
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        // ToDo: Store agent ID mapping if needed
    }
}

void SessionsFeedWidget::setupCustomGroupsUi()
{
    if (!treeView())
        return;

    treeView()->setDragDropMode(QAbstractItemView::DragDrop);
    treeView()->setDefaultDropAction(Qt::MoveAction);
    treeView()->setDragEnabled(true);
    treeView()->setAcceptDrops(true);
    treeView()->setDropIndicatorShown(true);

    if (!btnGroupManager) {
        btnGroupManager = new QPushButton(this);
        btnGroupManager->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        btnGroupManager->setToolTip("Manage custom groups");
        btnGroupManager->setFixedSize(28, 28);
        addToolbarWidgetAfter(btnGroupManager);
    }
}

void SessionsFeedWidget::wireGroupingProxy(GroupingProxyModel* model)
{
    if (!model || !adaptixWidget)
        return;

    if (groupPopup) {
        groupPopup->deleteLater();
        groupPopup = nullptr;
    }
    groupPopup = new GroupManagerPopup(model, adaptixWidget, QStringLiteral("agents"), btnGroupManager, this);
    if (btnGroupManager) {
        disconnect(btnGroupManager, nullptr, nullptr, nullptr);
        connect(btnGroupManager, &QPushButton::clicked, groupPopup, &GroupManagerPopup::showPopup);
    }
    connect(model, &GroupingProxyModel::groupStructureChanged, groupPopup, &GroupManagerPopup::Rebuild);
    connect(model, &GroupingProxyModel::agentsDroppedOnGroup, this,
        [this](const QList<QPair<qint64, qint64>>& moves, qint64 toGroupId) {
            AuthProfile* profile = adaptixWidget->GetProfile();
            if (!profile)
                return;
            const int64_t to = (toGroupId < 0) ? 0 : static_cast<int64_t>(toGroupId);
            for (const auto& [agentId, fromGroupId] : moves) {
                const int64_t from = (fromGroupId < 0) ? 0 : static_cast<int64_t>(fromGroupId);
                HttpReqGroupMoveMemberAsync(agentId, from, to, *profile, [](bool ok, const QString& message, const QJsonObject&) {
                    if (!ok)
                        MessageError(message.isEmpty() ? QStringLiteral("Failed to move agent between groups") : message);
                });
            }
        });
    connect(model, &GroupingProxyModel::groupReparented, this,
        [this](int64_t groupId, int64_t newParentId) {
            AuthProfile* profile = adaptixWidget->GetProfile();
            if (!profile)
                return;
            const int64_t parent = (newParentId < 0) ? 0 : newParentId;
            HttpReqGroupReparentAsync(groupId, parent, *profile, [](bool ok, const QString& message, const QJsonObject&) {
                if (!ok)
                    MessageError(message.isEmpty() ? QStringLiteral("Failed to reparent group") : message);
            });
        });
}

void SessionsFeedWidget::rebuildModelChain()
{
    ListFeedWidget::rebuildModelChain();
    wireGroupingProxy(groupingProxy());
}

void SessionsFeedWidget::onGroupModeChanged(int index)
{
    auto* gp = groupingProxy();
    if (!gp)
        return;

    switch (index) {
        case 0:  gp->setViewMode(VM_Flat); break;
        case 1:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByDomain); break;
        case 2:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByComputer); break;
        case 3:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByUser); break;
        case 4:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByTag); break;
        case 5:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByListener); break;
        case 6:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByOs); break;
        case 7:  gp->setViewMode(VM_AutoGroup); gp->setAutoGroupField(AG_ByAgentType); break;
        case 8:  gp->setViewMode(VM_CustomGroups); break;
        case 9:  gp->setViewMode(VM_Pivots); break;
        default: gp->setViewMode(VM_Flat); break;
    }

    const bool treeMode = gp->viewMode() != VM_Flat;
    if (treeView()) {
        treeView()->setRootIsDecorated(treeMode);
        treeView()->setUniformRowHeights(!treeMode);
        treeView()->setExpandsOnDoubleClick(treeMode);
        if (treeMode)
            treeView()->expandAll();
    }
}

void SessionsFeedWidget::onFilterChanged()
{
    if (!filterModel)
        return;

    if (searchInput())
        filterModel->setTextFilter(searchInput()->text());

    if (activeFilter())
        filterModel->setOnlyActive(activeFilter()->isChecked());

    if (filterCombo() && filterCombo()->currentIndex() > 0) {
        QSet<QString> types;
        types.insert(filterCombo()->currentText());
        filterModel->setAgentTypes(types);
    } else {
        filterModel->setAgentTypes({});
    }

    filterModel->invalidate();
}

void SessionsFeedWidget::onSortingChanged(int index)
{
    if (!feedBlockModel || index == 0)
        return;

    Qt::SortOrder order = isSortAscending() ? Qt::AscendingOrder : Qt::DescendingOrder;

    switch (index) {
        case 1:  feedBlockModel->sortByFieldNumeric(SessionsBlock::Right, "dateNum", order); break;
        case 2:  feedBlockModel->sortByField(SessionsBlock::Id, "firstTag", order); break;
        case 3:  feedBlockModel->sortByField(SessionsBlock::Right, "status", order); break;
        case 4:  feedBlockModel->sortByFieldNumeric(SessionsBlock::Id, "idNum", order); break;
        case 5:  feedBlockModel->sortByField(SessionsBlock::Main, "agentType", order); break;
        case 6:  feedBlockModel->sortByField(SessionsBlock::Main, "listener", order); break;
        case 7:  feedBlockModel->sortByField(SessionsBlock::Main, "user", order); break;
        case 8:  feedBlockModel->sortByField(SessionsBlock::Main, "computer", order); break;
        case 9:  feedBlockModel->sortByField(SessionsBlock::Main, "domain", order); break;
        case 10: feedBlockModel->sortByFieldNumeric(SessionsBlock::Main, "ipNum", order); break;
        case 11: feedBlockModel->sortByField(SessionsBlock::Main, "process", order); break;
    }

    rebuildIdMap();
}

void SessionsFeedWidget::rebuildIdMap()
{
    agentIdToRow.clear();
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        qint64 id = r.entityId;
        if (id > 0)
            agentIdToRow[id] = i;
    }
}

void SessionsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
}

void SessionsFeedWidget::AddAgentItem(Agent* agent)
{
    if (!agent || !agent->show)
        return;

    auto existing = agentIdToRow.find(agent->data.Id);
    if (existing != agentIdToRow.end()) {
        feedBlockModel->updateRow(existing.value(), agentToFeedRow(agent));
        return;
    }

    feedBlockModel->insertRow(0, agentToFeedRow(agent));
    for (auto i = agentIdToRow.begin(); i != agentIdToRow.end(); ++i)
        i.value()++;
    agentIdToRow[agent->data.Id] = 0;
}

void SessionsFeedWidget::UpdateAgentItem(const AgentData& oldData, const Agent* agent)
{
    Q_UNUSED(oldData);
    if (!agent) return;

    auto it = agentIdToRow.find(agent->data.Id);
    if (it == agentIdToRow.end())
        return;

    FeedRow row = agentToFeedRow(agent);
    feedBlockModel->updateRow(it.value(), row);
}

void SessionsFeedWidget::RemoveAgentItem(qint64 agentId)
{
    if (groupingProxy())
        groupingProxy()->removeAgentFromAllCustomGroups(agentId);

    auto it = agentIdToRow.find(agentId);
    if (it == agentIdToRow.end())
        return;

    int row = it.value();
    feedBlockModel->removeRow(row);
    agentIdToRow.remove(agentId);

    for (auto i = agentIdToRow.begin(); i != agentIdToRow.end(); ++i) {
        if (i.value() > row)
            i.value()--;
    }
}

void SessionsFeedWidget::UpdateData()
{
    feedBlockModel->clear();
    agentIdToRow.clear();

    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    for (auto* agent : adaptixWidget->AgentsMap.values()) {
        if (!agent || agent->data.Id == 0 || !agent->show)
            continue;

        FeedRow row = agentToFeedRow(agent);
        feedBlockModel->addRow(row);
        agentIdToRow[agent->data.Id] = feedBlockModel->size() - 1;
    }

    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate) delegate->updateMaxWidths(feedBlockModel);
}

void SessionsFeedWidget::UpdateLastColumn(const QList<qint64>& agentIds)
{
    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    for (qint64 agentId : agentIds) {
        auto it = agentIdToRow.find(agentId);
        if (it == agentIdToRow.end())
            continue;

        Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (!agent)
            continue;

        FeedRow row = agentToFeedRow(agent);
        feedBlockModel->updateRow(it.value(), row);
    }
}

void SessionsFeedWidget::UpdateColumnsVisible()
{
    if (!feedBlockModel || !adaptixWidget)
        return;

    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    for (auto it = agentIdToRow.begin(); it != agentIdToRow.end(); ++it) {
        Agent* agent = adaptixWidget->AgentsMap.value(it.key(), nullptr);
        if (!agent)
            continue;
        feedBlockModel->updateRow(it.value(), agentToFeedRow(agent));
    }

    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate)
        delegate->updateMaxWidths(feedBlockModel);
}

void SessionsFeedWidget::UpdateAgentTypeComboBox()
{
    QSet<QString> types;
    for (auto* agent : adaptixWidget->AgentsMap.values()) {
        if (agent && !agent->data.Name.isEmpty())
            types.insert(agent->data.Name);
    }
    filterCombo()->blockSignals(true);
    QString current = filterCombo()->currentText();
    filterCombo()->clear();
    filterCombo()->addItem("All types");
    for (const auto& t : types)
        filterCombo()->addItem(t);
    int idx = filterCombo()->findText(current);
    if (idx >= 0) filterCombo()->setCurrentIndex(idx);
    filterCombo()->blockSignals(false);
}

void SessionsFeedWidget::Clear()
{
    feedBlockModel->clear();
    agentIdToRow.clear();
    if (groupingProxy())
        groupingProxy()->clearCustomGroups();
    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        filterCombo()->clear();
        filterCombo()->addItem("All types");
        filterCombo()->blockSignals(false);
    }
    if (groupCombo()) groupCombo()->setCurrentIndex(0);
}

void SessionsFeedWidget::OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members)
{
    groupingProxy()->addCustomGroup(groupId, parentId, name, members);
}

void SessionsFeedWidget::OnGroupRenamed(int64_t groupId, const QString& name)
{
    groupingProxy()->renameCustomGroup(groupId, name);
}

void SessionsFeedWidget::OnGroupDeleted(int64_t groupId)
{
    groupingProxy()->deleteCustomGroup(groupId);
}

void SessionsFeedWidget::OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove)
{
    groupingProxy()->setCustomGroupMembers(groupId, add, remove);
}

void SessionsFeedWidget::OnGroupReparented(int64_t groupId, int64_t newParentId)
{
    groupingProxy()->reparentCustomGroup(groupId, newParentId);
}

void SessionsFeedWidget::handleFeedMenu(const QPoint& pos)
{
    oclero::qlementine::Menu ctxMenu;
    QModelIndex index = prepareContextMenuSelection(pos);

    if (index.isValid() && groupingProxy()->data(index, Qt::UserRole).toLongLong() != 0) {
        QList<qint64> agentIds;
        QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
        for (const QModelIndex& proxyIndex : selectedRows) {
            if (proxyIndex.data(Qt::UserRole).toLongLong() == 0) continue;
            agentIds.append(proxyIndex.data(Qt::UserRole).toLongLong());
        }
        if (agentIds.isEmpty()) {
            qint64 id = groupingProxy()->data(index, Qt::UserRole).toLongLong();
            if (id != 0)
                agentIds.append(id);
        }
        if (agentIds.isEmpty()) return;

        auto agentMenu = ctxMenu.addMenu(QIcon(":/icons/agent"), "Agent");
        agentMenu->addAction(QIcon(":/icons/keyboard_command"), "Execute command", this, &SessionsFeedWidget::actionExecuteCommand);
        agentMenu->addAction(QIcon(":/icons/job"), "Task manager", this, &SessionsFeedWidget::actionTasksOpen);
        agentMenu->addSeparator();
        int agentCount = adaptixWidget->ScriptManager->AddMenuSession(agentMenu, "SessionAgent", agentIds);
        if (agentCount > 0)
            agentMenu->addSeparator();
        agentMenu->addAction(QIcon(":/icons/delete"), "Remove console data", this, &SessionsFeedWidget::actionConsoleDelete);
        agentMenu->addAction(QIcon(":/icons/delete"), "Remove from server", this, &SessionsFeedWidget::actionAgentRemove);

        auto sessionMenu = ctxMenu.addMenu(QIcon(":/icons/settings"), "Session");
        sessionMenu->addAction(QIcon(":/icons/wifi_signal"), "Mark as Active", this, &SessionsFeedWidget::actionMarkActive);
        sessionMenu->addAction(QIcon(":/icons/wifi_null"), "Mark as Inactive", this, &SessionsFeedWidget::actionMarkInactive);
        sessionMenu->addSeparator();
        if (agentIds.size() == 1)
            sessionMenu->addAction(QIcon(":/icons/info"), "Set data", this, &SessionsFeedWidget::actionSetData);
        sessionMenu->addAction(QIcon(":/icons/tag"), "Set tag", this, &SessionsFeedWidget::actionSetTag);
        sessionMenu->addSeparator();
        sessionMenu->addAction(QIcon(":/icons/color_fill"), "Set items color", this, &SessionsFeedWidget::actionItemColor);
        sessionMenu->addAction(QIcon(":/icons/color_text"), "Set text color", this, &SessionsFeedWidget::actionTextColor);
        sessionMenu->addAction(QIcon(":/icons/color_reset"), "Reset color", this, &SessionsFeedWidget::actionColorReset);
        sessionMenu->addSeparator();
        sessionMenu->addAction(QIcon(":/icons/visibility_off"), "Hide on client", this, &SessionsFeedWidget::actionHide);

        ctxMenu.addAction(QIcon(":/icons/interact"), "Interact", this, &SessionsFeedWidget::actionConsoleOpen);
        ctxMenu.addSeparator();
        ctxMenu.addMenu(agentMenu);

        auto browserMenu = ctxMenu.addMenu(QIcon(":/icons/open_folder"), "Browsers");
        int browserCount = adaptixWidget->ScriptManager->AddMenuSession(browserMenu, "SessionBrowser", agentIds);
        if (browserCount > 0)
            ctxMenu.addMenu(browserMenu);
        else ctxMenu.removeAction(browserMenu->menuAction());

        auto accessMenu = ctxMenu.addMenu(QIcon(":/icons/exchange"), "Access");
        int accessCount = adaptixWidget->ScriptManager->AddMenuSession(accessMenu, "SessionAccess", agentIds);
        if (accessCount > 0)
            ctxMenu.addMenu(accessMenu);
        else ctxMenu.removeAction(accessMenu->menuAction());

        adaptixWidget->ScriptManager->AddMenuSession(&ctxMenu, "SessionMain", agentIds);

        ctxMenu.addSeparator();
        ctxMenu.addMenu(sessionMenu);
    }
    ctxMenu.addAction(QIcon(":/icons/visibility"), "Show all items", this, &SessionsFeedWidget::actionItemsShowAll);
    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void SessionsFeedWidget::onItemDoubleClicked(const QModelIndex& index)
{
    qint64 agentId = index.data(Qt::UserRole).toLongLong();
    if (agentId > 0)
        adaptixWidget->LoadConsoleUI(agentId);
}

void SessionsFeedWidget::actionConsoleOpen()
{
    QModelIndex idx = treeView()->currentIndex();
    if (idx.isValid()) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId > 0)
            adaptixWidget->LoadConsoleUI(agentId);
    }
}

void SessionsFeedWidget::actionTasksOpen()
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid() || !feedBlockModel)
        return;

    QModelIndex srcIdx = idx;
    QAbstractItemModel* m = const_cast<QAbstractItemModel*>(idx.model());
    while (m && m != feedBlockModel) {
        auto* sp = qobject_cast<QSortFilterProxyModel*>(m);
        if (sp) {
            srcIdx = sp->mapToSource(srcIdx);
            m = sp->sourceModel();
            continue;
        }
        auto* gp = qobject_cast<GroupingProxyModel*>(m);
        if (gp) {
            srcIdx = gp->mapToSource(srcIdx);
            m = gp->sourceModel();
            continue;
        }
        break;
    }
    if (!srcIdx.isValid() || srcIdx.row() < 0 || srcIdx.row() >= feedBlockModel->size())
        return;

    const FeedRow& row = feedBlockModel->rowAt(srcIdx.row());
    qint64 agentId = row.entityId;
    if (agentId > 0) {
        adaptixWidget->TasksDock->SetAgentFilter(agentId);
        adaptixWidget->SetTasksUI();
    }
}

void SessionsFeedWidget::actionExecuteCommand()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }
    if (listId.empty())
        return;

    bool ok = false;
    QString cmd = QInputDialog::getText(this, "Execute Command", "Command", QLineEdit::Normal, "", &ok);
    if (!ok)
        return;

    QList<ConsoleWidget*> consoles;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (auto id : listId) {
            if (adaptixWidget->AgentsMap.contains(id))
                consoles.append(adaptixWidget->AgentsMap[id]->Console);
        }
    }
    for (auto* console : consoles) {
        console->SetInput(cmd);
        console->processInput();
    }
}

void SessionsFeedWidget::actionConsoleDelete()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear Confirmation",
        "Are you sure you want to delete all agent console data and history from server (tasks will not be deleted from TaskManager)?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId > 0) listId.append(agentId);
    }
    if (listId.empty()) return;

    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (auto id : listId) {
            if (adaptixWidget->AgentsMap.contains(id))
                adaptixWidget->AgentsMap[id]->Console->Clear();
        }
    }

    HttpReqConsoleRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsFeedWidget::actionAgentRemove()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Confirmation",
        "Are you sure you want to delete all information about the selected agents from the server?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId > 0) listId.append(agentId);
    }
    if (listId.empty()) return;

    HttpReqAgentRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsFeedWidget::actionMarkActive()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
    }
    if (listId.empty()) return;

    HttpReqAgentSetMarkAsync(listId, "", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsFeedWidget::actionMarkInactive()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
    }
    if (listId.empty()) return;

    HttpReqAgentSetMarkAsync(listId, "Inactive", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsFeedWidget::actionRemove()
{
    QModelIndexList selected = treeView()->selectionModel()->selectedIndexes();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (agent) agent->show = false;
    }
    UpdateData();
}

void SessionsFeedWidget::actionSetTag()
{
    QString tag;
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
        if (tag.isEmpty()) {
            QReadLocker locker(&adaptixWidget->AgentsMapLock);
            Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
            if (agent) tag = agent->data.Tags;
        }
    }
    if (listId.empty()) return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal, tag, &inputOk);
    if (inputOk) {
        HttpReqAgentSetTagAsync(listId, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsFeedWidget::actionSetData()
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid()) return;
    qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
    if (agentId == 0) return;

    AgentData agentData;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        if (!adaptixWidget->AgentsMap.contains(agentId)) return;
        agentData = adaptixWidget->AgentsMap[agentId]->data;
    }

    auto* dialog = new DialogAgentData();
    dialog->SetProfile(*(adaptixWidget->GetProfile()));
    dialog->SetAgentData(agentData);
    dialog->Start();
}

void SessionsFeedWidget::actionHide()
{
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        if (adaptixWidget->AgentsMap.contains(agentId))
            adaptixWidget->AgentsMap[agentId]->show = false;
    }
    locker.unlock();
    UpdateData();
}

void SessionsFeedWidget::actionItemsShowAll()
{
    for (auto* agent : adaptixWidget->AgentsMap.values())
        if (agent) agent->show = true;
    UpdateData();
}

void SessionsFeedWidget::actionItemColor()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
    }
    if (listId.empty()) return;

    QColor itemColor = QColorDialog::getColor(Qt::white, nullptr, "Select items color");
    if (itemColor.isValid()) {
        HttpReqAgentSetColorAsync(listId, itemColor.name(), "", false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsFeedWidget::actionTextColor()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
    }
    if (listId.empty()) return;

    QColor textColor = QColorDialog::getColor(Qt::white, nullptr, "Select text color");
    if (textColor.isValid()) {
        HttpReqAgentSetColorAsync(listId, "", textColor.name(), false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsFeedWidget::actionColorReset()
{
    QList<qint64> listId;
    QModelIndexList selected = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 agentId = groupingProxy()->data(idx, Qt::UserRole).toLongLong();
        if (agentId == 0) continue;
        listId.append(agentId);
    }
    if (listId.empty()) return;

    HttpReqAgentSetColorAsync(listId, "", "", true, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
