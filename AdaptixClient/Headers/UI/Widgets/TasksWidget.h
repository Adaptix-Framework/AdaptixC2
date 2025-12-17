#ifndef ADAPTIXCLIENT_TASKSWIDGET_H
#define ADAPTIXCLIENT_TASKSWIDGET_H

#include <main.h>
#include <Utils/CustomElements.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/AdaptixWidget.h>

#include <QSortFilterProxyModel>

class Task;
class AdaptixWidget;

enum TasksColumns {
    TC_TaskId,
    TC_TaskType,
    TC_AgentId,
    TC_Client,
    TC_User,
    TC_Computer,
    TC_StartTime,
    TC_FinishTime,
    TC_CommandLine,
    TC_Result,
    TC_Output,
    TC_ColumnCount
};



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



class TasksFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    QString agentFilter;
    QString statusFilter;
    QString textFilter;

public:
    explicit TasksFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setSortRole(Qt::UserRole);
    };

    void setAgentFilter(const QString &agent) {
        agentFilter = agent;
        invalidateFilter();
    }
    void setStatusFilter(const QString &status) {
        statusFilter = status;
        invalidateFilter();
    }
    void setTextFilter(const QString &text){
        textFilter = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(const int row, const QModelIndex &parent) const override {
        auto model = sourceModel();
        if (!model)
            return true;

        QString agent  = model->index(row, TC_AgentId, parent).data().toString();
        QString status = model->index(row, TC_Result, parent).data().toString();

        if (!agentFilter.isEmpty() && agentFilter != "All agents" && agent != agentFilter)
            return false;

        if (!statusFilter.isEmpty() && statusFilter != "Any status" && status != statusFilter)
            return false;

        if (!textFilter.isEmpty()) {
            const int colCount = model->columnCount();
            QRegularExpression re(QRegularExpression::escape(textFilter), QRegularExpression::CaseInsensitiveOption);
            bool matched = false;
            for (int col = 0; col < colCount; ++col) {
                QString val = model->index(row, col, parent).data().toString();
                if (val.contains(re)) {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return false;
        }

        return true;
    }
};




class TasksTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<TaskData>     tasks;
    QHash<QString, int>   idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < tasks.size(); ++i)
            idToRow[tasks[i].TaskId] = i;
    }

public:
    explicit TasksTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return tasks.size(); }
    int rowCount() const { return tasks.size(); }

    int columnCount(const QModelIndex&) const override { return TC_ColumnCount; }
    int columnCount() const { return TC_ColumnCount; }

    QVariant data(const QModelIndex& index, const int role) const override {
        if (!index.isValid() || index.row() >= tasks.size())
            return {};

        const TaskData& t = tasks.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case TC_TaskId:      return t.TaskId;
                case TC_TaskType:    return t.TaskType == 1 ? "TASK" :
                                     t.TaskType == 3 ? "JOB" :
                                     t.TaskType == 4 ? "TUNNEL" : "unknown";
                case TC_AgentId:     return t.AgentId;
                case TC_Client:      return t.Client;
                case TC_User:        return t.User;
                case TC_Computer:    return t.Computer;
                case TC_StartTime:   return UnixTimestampGlobalToStringLocal(t.StartTime);
                case TC_FinishTime:  return UnixTimestampGlobalToStringLocal(t.FinishTime);
                case TC_CommandLine: return t.CommandLine;
                case TC_Result:      return t.Status;
                case TC_Output:      return t.Message;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case TC_StartTime:   return t.StartTime;
                case TC_FinishTime:  return t.FinishTime;
                default:             return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case TC_TaskId:
                case TC_TaskType:
                case TC_AgentId:
                case TC_Client:
                case TC_User:
                case TC_Computer:
                case TC_StartTime:
                case TC_FinishTime:
                case TC_Result:
                    return Qt::AlignCenter;
            }
        }

        if (role == Qt::ForegroundRole && index.column() == TC_Result) {
            if (t.Status == "Error")    return QColor("#E34234");
            if (t.Status == "Canceled") return QColor("#89CFF0");
            if (t.Status == "Success")  return QColor("#39FF14");
        }

        return {};
    }

    QVariant headerData(const int section, const Qt::Orientation o, const int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "Task ID","Type","Agent ID","Client","User",
            "Computer","Start Time","Finish Time","Command Line","Status","Message"
        };

        return headers.value(section);
    }

    void add(const TaskData& task) {
        const int row = tasks.size();
        beginInsertRows(QModelIndex(), row, row);
        tasks.append(task);
        idToRow[task.TaskId] = row;
        endInsertRows();
    }

    void add(const QList<TaskData>& list) {
        if (list.isEmpty())
            return;

        const int start = tasks.size();
        const int end   = start + list.size() - 1;

        beginInsertRows(QModelIndex(), start, end);
        for (const auto& item : list) {
            idToRow[item.TaskId] = tasks.size();
            tasks.append(item);
        }
        endInsertRows();
    }

    void update(const QString& taskId, const TaskData& newData) {
        auto it = idToRow.find(taskId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        tasks[row] = newData;
        Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1));
    }

    void remove(const QString &taskId) {
        auto it = idToRow.find(taskId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        idToRow.remove(taskId);
        tasks.remove(row);
        endRemoveRows();

        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        tasks.clear();
        idToRow.clear();
        endResetModel();
    }
};



class TasksWidget : public QWidget
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;

    KDDockWidgets::QtWidgets::DockWidget* dockWidgetTable;
    KDDockWidgets::QtWidgets::DockWidget* dockWidgetOutput;

    QGridLayout* mainGridLayout = nullptr;
    QTableView*  tableView      = nullptr;
    QShortcut*   shortcutSearch = nullptr;

    TasksTableModel*       tasksModel = nullptr;
    TasksFilterProxyModel* proxyModel = nullptr;

    QWidget*        searchWidget = nullptr;
    QHBoxLayout*    searchLayout = nullptr;
    QComboBox*      comboAgent   = nullptr;
    QComboBox*      comboStatus  = nullptr;
    QLineEdit*      inputFilter  = nullptr;
    ClickableLabel* hideButton   = nullptr;

    bool showPanel = false;

    void createUI();

public:
    TaskOutputWidget* taskOutputConsole = nullptr;

    explicit TasksWidget( AdaptixWidget* w );
    ~TasksWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dockTasks();
    KDDockWidgets::QtWidgets::DockWidget* dockTasksOutput();

    void SetUpdatesEnabled(const bool enabled);

    void AddTaskItem(TaskData newTask) const;
    void UpdateTaskItem(const QString &taskId, const TaskData &task) const;
    void RemoveTaskItem(const QString &taskId) const;
    void RemoveAgentTasksItem(const QString &agentId) const;

    void SetAgentFilter(const QString &agentId);
    void UpdateColumnsVisible() const;
    void UpdateColumnsSize() const;
    void Clear() const;

public Q_SLOTS:
    void toggleSearchPanel();
    void handleTasksMenu( const QPoint &pos );
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const;
    void onFilterChanged() const;
    void actionCopyTaskId() const;
    void actionCopyCmd() const;
    void actionOpenConsole() const;
    void actionCancel() const;
    void actionDelete() const;
};

#endif
