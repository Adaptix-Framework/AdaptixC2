#include <Agent/TableWidgetItemAgent.h>

TableWidgetItemAgent::TableWidgetItemAgent(const QString &text, Agent *agent)
{
    this->agent = agent;
    setText(text);
    setTextAlignment( Qt::AlignCenter );
    setFlags( flags() ^ Qt::ItemIsEditable );
}

TableWidgetItemAgent::~TableWidgetItemAgent() = default;

void TableWidgetItemAgent::RevertColor()
{
    setBackground(QBrush());
    setForeground(QBrush());
}

void TableWidgetItemAgent::SetColor(QColor bg, QColor fg)
{
    if (bg.isValid())
        this->setBackground(bg);

    if (fg.isValid())
        this->setForeground(fg);
}
