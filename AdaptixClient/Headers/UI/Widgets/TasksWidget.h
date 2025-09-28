#ifndef ADAPTIXCLIENT_TASKSWIDGET_H
#define ADAPTIXCLIENT_TASKSWIDGET_H

#include <main.h>
#include <Utils/CustomElements.h>
#include <UI/Widgets/AbstractDock.h>

class Task;
class AdaptixWidget;

class TaskOutputWidget : public QWidget
{
    QGridLayout* mainGridLayout = nullptr;
    QLabel*      label          = nullptr;
    QLineEdit*   inputMessage   = nullptr;
    QTextEdit*   outputTextEdit = nullptr;

    void createUI();

public:
    explicit TaskOutputWidget( );
    ~TaskOutputWidget() override;

    void SetConten(const QString &message, const QString &text) const;
};



class TasksWidget : public QWidget
{
    AdaptixWidget* adaptixWidget  = nullptr;

    KDDockWidgets::QtWidgets::DockWidget* dockWidgetTable;
    KDDockWidgets::QtWidgets::DockWidget* dockWidgetOutput;

    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    QWidget*        searchWidget = nullptr;
    QHBoxLayout*    searchLayout = nullptr;
    QComboBox*      comboAgent   = nullptr;
    QComboBox*      comboStatus  = nullptr;
    QLineEdit*      inputFilter  = nullptr;
    ClickableLabel* hideButton   = nullptr;

    void createUI();
    bool filterItem(const TaskData &task) const;
    void addTableItem(const Task* newTask) const;

public:
    int ColumnTaskId      = 0;
    int ColumnTaskType    = 1;
    int ColumnAgentId     = 2;
    int ColumnClient      = 3;
    int ColumnUser        = 4;
    int ColumnComputer    = 5;
    int ColumnStartTime   = 6;
    int ColumnFinishTime  = 7;
    int ColumnCommandLine = 8;
    int ColumnResult      = 9;
    int ColumnOutput      = 10;

    bool showPanel = false;

    TaskOutputWidget* taskOutputConsole = nullptr;

    explicit TasksWidget( AdaptixWidget* w );
    ~TasksWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dockTasks();
    KDDockWidgets::QtWidgets::DockWidget* dockTasksOutput();

    void AddTaskItem(Task* newTask) const;
    void RemoveTaskItem(const QString &taskId) const;
    void RemoveAgentTasksItem(const QString &agentId) const;

    void SetAgentFilter(const QString &agentId);
    void SetData() const;
    void UpdateColumnsVisible() const;
    void ClearTableContent() const;
    void Clear() const;

public Q_SLOTS:
    void toggleSearchPanel();
    void handleTasksMenu( const QPoint &pos );
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const;
    void onAgentChange(QString agentId) const;
    void actionCopyTaskId() const;
    void actionCopyCmd() const;
    void actionOpenConsole() const;
    void actionCancel() const;
    void actionDelete() const;
};

#endif
