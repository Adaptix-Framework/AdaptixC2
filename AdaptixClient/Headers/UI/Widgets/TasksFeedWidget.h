#ifndef ADAPTIXCLIENT_TASKSFEEDWIDGET_H
#define ADAPTIXCLIENT_TASKSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

#include <QHelpEvent>

class AdaptixWidget;
class TaskOutputWidget;

class TasksFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidgetFeed   = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidgetOutput = nullptr;

    FeedListModel*    feedBlockModel = nullptr;
    PagedTableHelper* pageHelper     = nullptr;
    int               m_offset       = 0;
    QString           m_sortCol      = "StartDate";
    QString           m_sortOrder    = "desc";
    bool              cachePrimed    = false;
    mutable QHash<qint64, TaskData> m_taskCache;

    void setupPagination();
    void loadCurrentPage();
    QModelIndex mapToFeedSource(const QModelIndex& index) const;
    bool taskDataFromIndex(const QModelIndex& index, TaskData* out) const;
    void showTaskOutput(const QModelIndex& index) const;

    struct TaskInfo { qint64 taskId = 0; qint64 agentId = 0; QString cmdline; bool valid = false; };
    TaskInfo currentTaskInfo() const;

public:
    TaskOutputWidget* taskOutputConsole = nullptr;

    explicit TasksFeedWidget(AdaptixWidget* w);
    ~TasksFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dockTasks();
    KDDockWidgets::QtWidgets::DockWidget* dockTasksOutput();
    void SetUpdatesEnabled(bool enabled);
    void AddTaskItem(TaskData newTask);
    void UpdateTaskItem(qint64 taskId, const TaskData& task) const;
    void RemoveTaskItem(qint64 taskId);
    void RemoveAgentTasksItem(qint64 agentId);
    void SetAgentFilter(qint64 agentId);
    void UpdateColumnsSize() const {}
    void UpdateColumnsVisible() const;
    void UpdateFilterComboBoxes() const;
    void Clear();

    void onFilterChanged() override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void onItemSelection(const QModelIndex& current) const;
    void onItemDoubleClicked(const QModelIndex& index);
    void actionCopyTaskId() const;
    void actionCopyCmd() const;
    void actionOpenConsole() const;
    void actionCancel() const;
    void actionDelete() const;

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif
