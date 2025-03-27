#ifndef ADAPTIXCLIENT_TASKSWIDGET_H
#define ADAPTIXCLIENT_TASKSWIDGET_H

#include <main.h>
#include <Agent/Task.h>
#include <Utils/CustomElements.h>

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
    QWidget*      mainWidget     = nullptr;
    QGridLayout*  mainGridLayout = nullptr;
    QTableWidget* tableWidget    = nullptr;
    QShortcut*    shortcutSearch = nullptr;

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

    explicit TasksWidget( QWidget* w );
    ~TasksWidget() override;

    void AddTaskItem(Task* newTask) const;
    void RemoveTaskItem(const QString &taskId) const;
    void RemoveAgentTasksItem(const QString &agentId) const;

    void SetAgentFilter(const QString &agentId);
    void SetData() const;
    void ClearTableContent() const;
    void Clear() const;

public slots:
    void toggleSearchPanel();
    void handleTasksMenu( const QPoint &pos );
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const;
    void onAgentChange(QString agentId) const;
    void actionCopyTaskId() const;
    void actionCopyCmd() const;
    void actionOpenConsole() const;
    void actionStop() const;
    void actionDelete() const;
};

#endif //ADAPTIXCLIENT_TASKSWIDGET_H
