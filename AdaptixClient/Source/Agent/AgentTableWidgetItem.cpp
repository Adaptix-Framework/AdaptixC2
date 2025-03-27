#include <Agent/AgentTableWidgetItem.h>

AgentTableWidgetItem::AgentTableWidgetItem(const QString &text, Agent *agent)
{
    this->agent = agent;
    this->setText(text);
    this->setTextAlignment( Qt::AlignCenter );
    this->setFlags( flags() ^ Qt::ItemIsEditable );
}

AgentTableWidgetItem::~AgentTableWidgetItem() = default;

void AgentTableWidgetItem::RevertColor()
{
    this->setBackground(QBrush());
    this->setForeground(QBrush());
}

void AgentTableWidgetItem::SetColor(QColor bg, QColor fg)
{
    if (bg.isValid())
        this->setBackground(bg);

    if (fg.isValid())
        this->setForeground(fg);
}
