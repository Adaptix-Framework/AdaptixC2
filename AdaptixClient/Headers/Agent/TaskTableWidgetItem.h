#ifndef ADAPTIXCLIENT_TABLEWIDGETITEMTASK_H
#define ADAPTIXCLIENT_TABLEWIDGETITEMTASK_H

#include <main.h>

class Task;

class TaskTableWidgetItem final : public QTableWidgetItem
{
public:
    Task* task = nullptr;

    explicit TaskTableWidgetItem( const QString& text, Task* task );
    ~TaskTableWidgetItem() override;
};

#endif //ADAPTIXCLIENT_TABLEWIDGETITEMTASK_H
