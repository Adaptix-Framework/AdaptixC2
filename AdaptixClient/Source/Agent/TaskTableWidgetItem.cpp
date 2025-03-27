#include <Agent/TaskTableWidgetItem.h>

TaskTableWidgetItem::TaskTableWidgetItem(const QString &text, Task *task)
{
    this->task = task;
    this->setText(text);
    this->setTextAlignment( Qt::AlignCenter );
    this->setFlags( flags() ^ Qt::ItemIsEditable );
}

TaskTableWidgetItem::~TaskTableWidgetItem() = default;
