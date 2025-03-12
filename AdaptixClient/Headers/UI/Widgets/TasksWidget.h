#ifndef ADAPTIXCLIENT_TASKSWIDGET_H
#define ADAPTIXCLIENT_TASKSWIDGET_H

#include <main.h>
#include <Agent/Task.h>

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
    QComboBox*    comboAgent     = nullptr;
    QComboBox*    comboStatus    = nullptr;
    QLineEdit*    inputFilter    = nullptr;

    void createUI();
    bool filterItem(const TaskData &task) const;
    void addTableItem(const Task* newTask) const;

public:
    TaskOutputWidget* taskOutputConsole = nullptr;

    explicit TasksWidget( QWidget* w );
    ~TasksWidget() override;

    void Clear() const;
    void AddTaskItem(Task* newTask) const;
    void EditTaskSend(TaskData newTask) const;
    void RemoveTaskItem(const QString &taskId) const;
    void SetData() const;
    void SetAgentFilter(const QString &agentId) const;
    void ClearTableContent() const;

public slots:
    void handleTasksMenu( const QPoint &pos );
    void onTableItemSelection() const;
    void onAgentChange(QString agentId) const;
    void actionCopyTaskId() const;
    void actionCopyCmd() const;
    void actionOpenConsole() const;
    void actionStop() const;
    void actionDelete() const;
};

#endif //ADAPTIXCLIENT_TASKSWIDGET_H
