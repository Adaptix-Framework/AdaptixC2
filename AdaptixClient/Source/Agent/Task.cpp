#include <Agent/Task.h>
#include <UI/Widgets/ConsoleWidget.h>

Task::Task(QJsonObject jsonObjTaskData)
{
    this->data.TaskId      = jsonObjTaskData["a_task_id"].toString();
    this->data.TaskType    = jsonObjTaskData["a_task_type"].toDouble();
    this->data.AgentId     = jsonObjTaskData["a_id"].toString();
    this->data.StartTime   = jsonObjTaskData["a_start_time"].toDouble();
    this->data.CommandLine = jsonObjTaskData["a_cmdline"].toString();
    this->data.Client      = jsonObjTaskData["a_client"].toString();
    this->data.User        = jsonObjTaskData["a_user"].toString();
    this->data.Computer    = jsonObjTaskData["a_computer"].toString();
    this->data.FinishTime  = jsonObjTaskData["a_finish_time"].toDouble();
    this->data.MessageType = jsonObjTaskData["a_msg_type"].toDouble();
    this->data.Message     = jsonObjTaskData["a_message"].toString();
    this->data.Output      = jsonObjTaskData["a_text"].toString();
    this->data.Completed   = jsonObjTaskData["a_completed"].toBool();

    QString taskType = "";
    if ( this->data.TaskType == 1 )
        taskType = "TASK";
    else if ( this->data.TaskType == 3 )
        taskType = "JOB";
    else if ( this->data.TaskType == 4 )
        taskType = "TUNNEL";
    else
        taskType = "unknown";

    QString startTime  = UnixTimestampGlobalToStringLocal(this->data.StartTime);
    QString finishTime = UnixTimestampGlobalToStringLocal(this->data.FinishTime);

    this->item_TaskId      = new TaskTableWidgetItem( this->data.TaskId, this );
    this->item_TaskType    = new TaskTableWidgetItem( taskType, this );
    this->item_AgentId     = new TaskTableWidgetItem( this->data.AgentId, this );
    this->item_Client      = new TaskTableWidgetItem( this->data.Client, this );
    this->item_User        = new TaskTableWidgetItem( this->data.User, this );
    this->item_Computer    = new TaskTableWidgetItem( this->data.Computer, this );
    this->item_StartTime   = new TaskTableWidgetItem( startTime, this );
    this->item_FinishTime  = new TaskTableWidgetItem( finishTime, this );
    this->item_CommandLine = new TaskTableWidgetItem( this->data.CommandLine, this );
    this->item_Result      = new TaskTableWidgetItem( this->data.Status, this );
    this->item_Message     = new TaskTableWidgetItem( this->data.Message, this );

    item_CommandLine->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
    item_Message->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    this->data.Status = "Hosted";
    if (this->data.Completed) {
        if ( this->data.MessageType == CONSOLE_OUT_ERROR || this->data.MessageType == CONSOLE_OUT_LOCAL_ERROR ) {
            this->data.Status = "Error";
            this->item_Result->setForeground(QColor(COLOR_ChiliPepper));
        }
        else if ( this->data.MessageType == CONSOLE_OUT_INFO || this->data.MessageType == CONSOLE_OUT_LOCAL_INFO ) {
            this->data.Status = "Canceled";
            item_Result->setForeground(QColor(COLOR_BabyBlue) );
        }
        else {
            this->data.Status = "Success";
            item_Result->setForeground(QColor(COLOR_NeonGreen) );
        }
    }
    this->item_Result->setText(this->data.Status);
}

Task::~Task() = default;

void Task::Update(QJsonObject jsonObjTaskData)
{
    this->data.Completed   = jsonObjTaskData["a_completed"].toBool();
    if (this->data.Completed) {
        this->data.FinishTime  = jsonObjTaskData["a_finish_time"].toDouble();
        QString finishTime = UnixTimestampGlobalToStringLocal(this->data.FinishTime);
        this->item_FinishTime->setText(finishTime);

        this->data.MessageType = jsonObjTaskData["a_msg_type"].toDouble();
        if ( this->data.MessageType == CONSOLE_OUT_ERROR || this->data.MessageType == CONSOLE_OUT_LOCAL_ERROR ) {
            this->data.Status = "Error";
            this->item_Result->setForeground(QColor(COLOR_ChiliPepper));
        }
        else if ( this->data.MessageType == CONSOLE_OUT_INFO || this->data.MessageType == CONSOLE_OUT_LOCAL_INFO ) {
            this->data.Status = "Canceled";
            item_Result->setForeground(QColor(COLOR_BabyBlue) );
        }
        else {
            this->data.Status = "Success";
            item_Result->setForeground(QColor(COLOR_NeonGreen) );
        }
        this->item_Result->setText(this->data.Status);
    }

    if ( this->data.Message.isEmpty() ) {
        this->data.Message = jsonObjTaskData["a_message"].toString();
        this->item_Message->setText(this->data.Message);
    }

    this->data.Output += jsonObjTaskData["a_text"].toString();
}
