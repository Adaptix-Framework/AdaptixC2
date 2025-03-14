#ifndef TASK_H
#define TASK_H

#include <main.h>
#include <Agent/TaskTableWidgetItem.h>

class Task
{
public:
     TaskData data = {};

     TaskTableWidgetItem* item_TaskId      = nullptr;
     TaskTableWidgetItem* item_TaskType    = nullptr;
     TaskTableWidgetItem* item_AgentId     = nullptr;
     TaskTableWidgetItem* item_Client      = nullptr;
     TaskTableWidgetItem* item_User        = nullptr;
     TaskTableWidgetItem* item_Computer    = nullptr;
     TaskTableWidgetItem* item_StartTime   = nullptr;
     TaskTableWidgetItem* item_FinishTime  = nullptr;
     TaskTableWidgetItem* item_CommandLine = nullptr;
     TaskTableWidgetItem* item_Result      = nullptr;
     TaskTableWidgetItem* item_Message     = nullptr;

     explicit Task(QJsonObject jsonObjTaskData);
     ~Task();

     void    Update(QJsonObject jsonObjTaskData);
};

#endif //TASK_H
