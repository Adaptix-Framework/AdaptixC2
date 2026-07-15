#include <Agent/Agent.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/TaskOutputWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Settings.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QPainter>
#include <QDateTime>
#include <QToolTip>
#include <QHelpEvent>



namespace TasksBlock { enum { Id = 0, Main = 1, Right = 2, Count = 3 }; }

namespace TC { enum { TaskId = 0, TaskType, AgentId, Client, User, Computer, StartTime, FinishTime, CommandLine, Result, Count }; }

static bool tasksCol(int col)
{
    return GlobalClient && GlobalClient->settings && GlobalClient->settings->data.TasksTableColumns[col];
}

static QString tasksComputeStatus(const TaskData& t) {
    if (!t.Completed) {
        if (t.Status == QLatin1String("Running") || t.TaskType == 4)
            return QStringLiteral("Running");
        return QStringLiteral("Hosted");
    }
    if (t.MessageType == CONSOLE_OUT_ERROR || t.MessageType == CONSOLE_OUT_LOCAL_ERROR)
        return QStringLiteral("Error");
    if (t.MessageType == CONSOLE_OUT_INFO || t.MessageType == CONSOLE_OUT_LOCAL_INFO)
        return QStringLiteral("Canceled");
    return QStringLiteral("Success");
}

static QString tasksFormatDuration(qint64 startSec, qint64 finishSec) {
    if (startSec <= 0)
        return {};
    qint64 endSec = finishSec > 0 ? finishSec : QDateTime::currentSecsSinceEpoch();
    qint64 diff = endSec - startSec;
    if (diff < 0)
        diff = 0;
    if (diff < 60)
        return QString("%1s").arg(diff);
    if (diff < 3600)
        return QString("%1m %2s").arg(diff / 60).arg(diff % 60);
    return QString("%1h %2m").arg(diff / 3600).arg((diff % 3600) / 60);
}

static QString tasksTypeBadge(int taskType)
{
    if (taskType == 1) return QStringLiteral("TASK");
    if (taskType == 3) return QStringLiteral("JOB");
    if (taskType == 4) return QStringLiteral("TUNNEL");
    return QStringLiteral("?");
}

static FeedRow taskToFeedRow(const TaskData& t) {
    FeedRow row;
    row.resize(TasksBlock::Count);
    row.entityId = t.TaskId;

    const QString idStr = tasksCol(TC::TaskId) ? QString("#%1").arg(t.TaskId) : QString();
    const QString badge = tasksCol(TC::TaskType) ? tasksTypeBadge(t.TaskType) : QString();
    const QString date  = tasksCol(TC::StartTime)
        ? QDateTime::fromSecsSinceEpoch(t.StartTime).toString("dd/MM HH:mm:ss")
        : QString();
    row[TasksBlock::Id] = QVariantMap{{"id", idStr}, {"badge", badge}, {"date", date}};

    const QString mainText = tasksCol(TC::CommandLine) ? t.CommandLine : QString();

    QString secondText;
    {
        const QString client = tasksCol(TC::Client) ? t.Client : QString();
        const QString user = tasksCol(TC::User) ? t.User : QString();
        const QString computer = tasksCol(TC::Computer) ? t.Computer : QString();

        QString right;
        if (!user.isEmpty() && !computer.isEmpty())
            right = user + " @ " + computer;
        else if (!user.isEmpty())
            right = user;
        else if (!computer.isEmpty())
            right = computer;

        if (!client.isEmpty() && !right.isEmpty())
            secondText = client + " \u2192 " + right;
        else if (!client.isEmpty())
            secondText = client;
        else
            secondText = right;

        if (tasksCol(TC::AgentId) && t.AgentId > 0)
            secondText += QString(" (#%1)").arg(t.AgentId);
    }

    row[TasksBlock::Main] = QVariantMap{
        {"main", mainText},
        {"submain", QString()},
        {"second", secondText},
        {"cmdline", t.CommandLine}
    };

    QString status;
    QString statusType;
    if (tasksCol(TC::Result)) {
        status = tasksComputeStatus(t);
        if (status == "Success") statusType = "success";
        else if (status == "Error") statusType = "error";
        else if (status == "Running") statusType = "running";
        else if (status == "Hosted") statusType = "hosted";
        else if (status == "Canceled") statusType = "canceled";
    }

    const QString rightSecond = (tasksCol(TC::FinishTime) && t.FinishTime > 0)
        ? QDateTime::fromSecsSinceEpoch(t.FinishTime).toString("dd/MM HH:mm:ss")
        : QString();

    row[TasksBlock::Right] = QVariantMap{
        {"main", QString()},
        {"second", rightSecond},
        {"status", status},
        {"statusType", statusType},
        {"dateNum", t.StartTime},
        {"agentId", t.AgentId}
    };
    row.isDead = false;
    return row;
}

static ListFeedDelegate* createTasksDelegate(QObject* parent) {
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new StatusBlock());
    return d;
}



TasksFeedWidget::TasksFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createTasksDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    setModel(feedBlockModel);
    setDelegate(delegate);
    rebuildModelChain();

    enableSearch(true);
    enableAutoCheck(true);
    enableFilterCombo(true, "All agents");
    enableActiveFilter(true, "in process");
    if (activeFilter()) {
        activeFilter()->setToolTip("Show only incomplete tasks (Hosted / Running)");
        if (GlobalClient && GlobalClient->settings)
            activeFilter()->setChecked(GlobalClient->settings->data.TasksInProcessOnly);
    }
    finalizeSearchWidget();

    enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        setCompactMode(GlobalClient->settings->data.TasksCompactMode);
    setBlockGap(12);

    enablePagination(true);

    taskOutputConsole = new TaskOutputWidget();

    dockWidgetFeed = new KDDockWidgets::QtWidgets::DockWidget( "TasksFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidgetFeed->setTitle("Tasks");
    dockWidgetFeed->setWidget(this);
    dockWidgetFeed->setIcon(QIcon(":/icons/job"), KDDockWidgets::IconPlace::TabBar);

    dockWidgetOutput = new KDDockWidgets::QtWidgets::DockWidget( "TaskOutput:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidgetOutput->setTitle("Task Output");
    dockWidgetOutput->setWidget(taskOutputConsole);
    dockWidgetOutput->setIcon(QIcon(":/icons/job"), KDDockWidgets::IconPlace::TabBar);

    setupPagination();

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &TasksFeedWidget::handleFeedMenu);
    connect(treeView(), &QTreeView::doubleClicked, this, &TasksFeedWidget::onItemDoubleClicked);
    connect(treeView()->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &TasksFeedWidget::onItemSelection);
}

TasksFeedWidget::~TasksFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* TasksFeedWidget::dockTasks() { return dockWidgetFeed; }
KDDockWidgets::QtWidgets::DockWidget* TasksFeedWidget::dockTasksOutput() { return dockWidgetOutput; }

void TasksFeedWidget::setupPagination()
{
    pageHelper = new PagedTableHelper(m_adaptixWidget->GetProfile(), "/agent/task/list", this);
    pageHelper->setPageSize(paginationBar()->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady, this, &TasksFeedWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred, this, &TasksFeedWidget::onPageError);
    connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
        paginationBar()->setLoading(loading);
        treeView()->setEnabled(!loading);
    });
    connect(paginationBar(), &PaginationBar::prevClicked, this, [this]() {
        m_offset = qMax(0, m_offset - paginationBar()->pageSize());
        loadCurrentPage();
    });
    connect(paginationBar(), &PaginationBar::nextClicked, this, [this]() {
        m_offset += paginationBar()->pageSize();
        loadCurrentPage();
    });
    connect(paginationBar(), &PaginationBar::pageSizeChanged, this, [this](int size) {
        pageHelper->setPageSize(size);
        m_offset = 0;
        loadCurrentPage();
    });
}

void TasksFeedWidget::loadCurrentPage()
{
    if (searchInput())
        pageHelper->setParam("q", searchInput()->text());
    if (filterCombo() && filterCombo()->currentIndex() > 0)
        pageHelper->setParam("agent_id", filterCombo()->currentData().toString());
    else
        pageHelper->setParam("agent_id", "");
    if (activeFilter() && activeFilter()->isChecked())
        pageHelper->setParam("completed", "0");
    else
        pageHelper->setParam("completed", "");
    pageHelper->setParam("sort", m_sortCol);
    pageHelper->setParam("order", m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void TasksFeedWidget::onFilterChanged()
{
    const QObject* s = sender();
    if (!s || s == activeFilter() || s == filterCombo() || (autoAction() && autoAction()->isChecked())) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void TasksFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
    if (enabled && !cachePrimed)
        loadCurrentPage();
}

void TasksFeedWidget::AddTaskItem(TaskData newTask)
{
    if (!feedBlockModel)
        return;

    if (m_adaptixWidget) {
        QWriteLocker locker(&m_adaptixWidget->TasksMapLock);
        m_adaptixWidget->TasksMap[newTask.TaskId] = newTask;
    }

    if (activeFilter() && activeFilter()->isChecked() && newTask.Completed)
        return;

    if (filterCombo() && filterCombo()->currentIndex() > 0
        && filterCombo()->currentData().toLongLong() != newTask.AgentId)
        return;

    if (m_taskCache.contains(newTask.TaskId)) {
        UpdateTaskItem(newTask.TaskId, newTask);
        return;
    }

    m_taskCache[newTask.TaskId] = newTask;
    if (m_offset == 0)
        feedBlockModel->insertRow(0, taskToFeedRow(newTask));
}

void TasksFeedWidget::UpdateTaskItem(qint64 taskId, const TaskData& task) const
{
    if (!feedBlockModel)
        return;

    const bool inProcessOnly = activeFilter() && activeFilter()->isChecked();
    if (inProcessOnly && task.Completed) {
        bool wasSelected = false;
        {
            TaskData cur;
            if (taskOutputConsole && taskDataFromIndex(treeView()->currentIndex(), &cur) && cur.TaskId == taskId)
                wasSelected = true;
        }
        m_taskCache.remove(taskId);
        for (int i = 0; i < feedBlockModel->size(); ++i) {
            if (feedBlockModel->rowAt(i).entityId == taskId) {
                feedBlockModel->removeRow(i);
                break;
            }
        }
        if (wasSelected && taskOutputConsole)
            taskOutputConsole->SetConten(task.Message, task.Output);
        return;
    }

    m_taskCache[taskId] = task;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        if (feedBlockModel->rowAt(i).entityId == taskId) {
            feedBlockModel->updateRow(i, taskToFeedRow(task));
            if (taskOutputConsole) {
                TaskData cur;
                if (taskDataFromIndex(treeView()->currentIndex(), &cur) && cur.TaskId == taskId)
                    taskOutputConsole->SetConten(task.Message, task.Output);
            }
            return;
        }
    }

    if (!task.Completed && m_offset == 0) {
        if (filterCombo() && filterCombo()->currentIndex() > 0
            && filterCombo()->currentData().toLongLong() != task.AgentId)
            return;
        feedBlockModel->insertRow(0, taskToFeedRow(task));
    }
}

void TasksFeedWidget::RemoveTaskItem(qint64 taskId)
{
    if (!feedBlockModel)
        return;
    m_taskCache.remove(taskId);
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == taskId) {
            feedBlockModel->removeRow(i);
            return;
        }
    }
}

void TasksFeedWidget::RemoveAgentTasksItem(qint64 agentId)
{
    if (!feedBlockModel) return;
    for (auto it = m_taskCache.begin(); it != m_taskCache.end(); ) {
        if (it.value().AgentId == agentId)
            it = m_taskCache.erase(it);
        else
            ++it;
    }
    for (int i = feedBlockModel->size() - 1; i >= 0; --i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        qint64 rowAgentId = r.blockData[TasksBlock::Right].toMap().value("agentId").toLongLong();
        if (rowAgentId == agentId)
            feedBlockModel->removeRow(i);
    }
    loadCurrentPage();
}

void TasksFeedWidget::UpdateColumnsVisible() const
{
    if (!feedBlockModel)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        auto it = m_taskCache.constFind(r.entityId);
        if (it == m_taskCache.constEnd())
            continue;
        feedBlockModel->updateRow(i, taskToFeedRow(it.value()));
    }

    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate)
        delegate->updateMaxWidths(feedBlockModel);
}

void TasksFeedWidget::SetAgentFilter(qint64 agentId)
{
    if (filterCombo()) {
        for (int i = 0; i < filterCombo()->count(); ++i) {
            if (filterCombo()->itemData(i).toLongLong() == agentId) {
                filterCombo()->setCurrentIndex(i);
                return;
            }
        }
    }
}

void TasksFeedWidget::UpdateFilterComboBoxes() const
{
    if (!filterCombo() || !m_adaptixWidget) return;

    QMap<qint64, QString> agents;
    {
        QReadLocker locker(&m_adaptixWidget->AgentsMapLock);
        for (auto it = m_adaptixWidget->AgentsMap.begin(); it != m_adaptixWidget->AgentsMap.end(); ++it) {
            Agent* agent = it.value();
            if (agent && !agent->data.Name.isEmpty())
                agents[it.key()] = QString("#%1 %2@%3").arg(it.key()).arg(agent->data.Username, agent->data.Computer);
        }
    }

    filterCombo()->blockSignals(true);
    qint64 currentId = filterCombo()->currentData().toLongLong();
    filterCombo()->clear();
    filterCombo()->addItem("All agents", static_cast<qlonglong>(0));
    for (auto it = agents.begin(); it != agents.end(); ++it)
        filterCombo()->addItem(it.value(), static_cast<qlonglong>(it.key()));
    for (int i = 0; i < filterCombo()->count(); ++i) {
        if (filterCombo()->itemData(i).toLongLong() == currentId) {
            filterCombo()->setCurrentIndex(i);
            break;
        }
    }
    filterCombo()->blockSignals(false);
}

void TasksFeedWidget::Clear()
{
    m_taskCache.clear();
    if (feedBlockModel)
        feedBlockModel->clear();
    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        filterCombo()->clear();
        filterCombo()->addItem("All agents");
        filterCombo()->blockSignals(false);
    }
}

QModelIndex TasksFeedWidget::mapToFeedSource(const QModelIndex& index) const
{
    QModelIndex idx = index;
    while (idx.isValid() && idx.model() && idx.model() != feedBlockModel) {
        auto* sortProxy = qobject_cast<const QSortFilterProxyModel*>(idx.model());
        if (sortProxy) {
            idx = sortProxy->mapToSource(idx);
            continue;
        }
        auto* groupProxy = qobject_cast<const GroupingProxyModel*>(idx.model());
        if (groupProxy) {
            idx = groupProxy->mapToSource(idx);
            continue;
        }
        break;
    }
    return idx;
}

bool TasksFeedWidget::taskDataFromIndex(const QModelIndex& index, TaskData* out) const
{
    if (!out || !feedBlockModel)
        return false;
    QModelIndex src = mapToFeedSource(index);
    if (!src.isValid() || src.row() < 0 || src.row() >= feedBlockModel->size())
        return false;
    const qint64 taskId = feedBlockModel->rowAt(src.row()).entityId;
    auto it = m_taskCache.constFind(taskId);
    if (it == m_taskCache.constEnd())
        return false;
    *out = it.value();
    return true;
}

void TasksFeedWidget::showTaskOutput(const QModelIndex& index) const
{
    if (!taskOutputConsole)
        return;
    TaskData t;
    if (!taskDataFromIndex(index, &t)) {
        taskOutputConsole->SetConten("", "");
        return;
    }
    taskOutputConsole->SetConten(t.Message, t.Output);
}

void TasksFeedWidget::handleFeedMenu(const QPoint& pos)
{
    QModelIndex index = prepareContextMenuSelection(pos);
    if (!index.isValid())
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction("Copy Task ID", this, &TasksFeedWidget::actionCopyTaskId);
    ctxMenu.addAction("Copy Command", this, &TasksFeedWidget::actionCopyCmd);
    ctxMenu.addSeparator();
    ctxMenu.addAction("Open Console", this, &TasksFeedWidget::actionOpenConsole);
    ctxMenu.addSeparator();
    ctxMenu.addAction("Cancel", this, &TasksFeedWidget::actionCancel);
    ctxMenu.addAction("Delete", this, &TasksFeedWidget::actionDelete);

    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void TasksFeedWidget::onItemSelection(const QModelIndex& current) const
{
    showTaskOutput(current);
    if (m_adaptixWidget && current.isValid())
        m_adaptixWidget->LoadTasksOutput();
}

void TasksFeedWidget::onItemDoubleClicked(const QModelIndex& index)
{
    showTaskOutput(index);
    if (m_adaptixWidget)
        m_adaptixWidget->LoadTasksOutput();
}

void TasksFeedWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value("items").toArray();
    QList<TaskData> newTasks;
    for (const auto& itemVal : items) {
        QJsonObject item = itemVal.toObject();
        TaskData t;
        t.TaskId = static_cast<qint64>(item.value("a_task_id").toInt());
        t.AgentId = static_cast<qint64>(item.value("a_id").toInt());
        t.Client = item.value("a_client").toString();
        t.User = item.value("a_user").toString();
        t.Computer = item.value("a_computer").toString();
        t.CommandLine = item.value("a_cmdline").toString();
        t.StartTime = static_cast<qint64>(item.value("a_start_time").toInt());
        t.FinishTime = static_cast<qint64>(item.value("a_finish_time").toInt());
        t.Completed = item.value("a_completed").toBool();
        t.MessageType = item.value("a_msg_type").toInt();
        t.Message = item.value("a_message").toString();
        t.Output = item.value("a_text").toString();
        t.TaskType = item.value("a_task_type").toInt();
        t.Status = tasksComputeStatus(t);
        newTasks.append(t);
    }

    m_taskCache.clear();
    feedBlockModel->clear();
    for (const auto& t : newTasks) {
        m_taskCache[t.TaskId] = t;
        FeedRow row = taskToFeedRow(t);
        feedBlockModel->addRow(row);
    }
    if (m_adaptixWidget) {
        QWriteLocker locker(&m_adaptixWidget->TasksMapLock);
        for (const auto& t : newTasks)
            m_adaptixWidget->TasksMap[t.TaskId] = t;
    }

    int total = response["total"].toInt();
    paginationBar()->setInfo(m_offset + 1, m_offset + newTasks.size(), total);
    paginationBar()->setPrevEnabled(m_offset > 0);
    paginationBar()->setNextEnabled(m_offset + newTasks.size() < total);

    cachePrimed = true;

    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate)
        delegate->updateMaxWidths(feedBlockModel);
}

void TasksFeedWidget::onPageError(const QString& message)
{
    Q_UNUSED(message);
    m_taskCache.clear();
    feedBlockModel->clear();
    paginationBar()->setInfo(0, 0, 0);
    paginationBar()->setPrevEnabled(false);
    paginationBar()->setNextEnabled(false);
    cachePrimed = false;
}

TasksFeedWidget::TaskInfo TasksFeedWidget::currentTaskInfo() const
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid() || !feedBlockModel)
        return {};

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
    if (!srcIdx.isValid() || srcIdx.row() < 0 || srcIdx.row() >= feedBlockModel->size()) return {};

    const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
    TaskInfo info;
    info.taskId = r.entityId;
    info.agentId = r.blockData[TasksBlock::Right].toMap()["agentId"].toLongLong();
    const QVariantMap mainMap = r.blockData[TasksBlock::Main].toMap();
    info.cmdline = mainMap.value("cmdline").toString();
    if (info.cmdline.isEmpty())
        info.cmdline = mainMap.value("main").toString();
    info.valid = info.taskId > 0;
    return info;
}

void TasksFeedWidget::actionCopyTaskId() const
{
    TaskInfo info = currentTaskInfo();
    if (info.valid)
        QApplication::clipboard()->setText(QString::number(info.taskId));
}

void TasksFeedWidget::actionCopyCmd() const
{
    TaskInfo info = currentTaskInfo();
    if (info.valid)
        QApplication::clipboard()->setText(info.cmdline);
}

void TasksFeedWidget::actionOpenConsole() const
{
    TaskInfo info = currentTaskInfo();
    if (info.valid && info.agentId > 0)
        m_adaptixWidget->LoadConsoleUI(info.agentId);
}

void TasksFeedWidget::actionCancel() const
{
    TaskInfo info = currentTaskInfo();
    if (info.valid && info.agentId > 0)
        HttpReqTaskCancelAsync(info.agentId, {info.taskId}, *(m_adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
}

void TasksFeedWidget::actionDelete() const
{
    TaskInfo info = currentTaskInfo();
    if (info.valid && info.agentId > 0)
        HttpReqTasksDeleteAsync(info.agentId, {info.taskId}, *(m_adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
}
