#include <Agent/TableWidgetItemAgent.h>

TableWidgetItemAgent::TableWidgetItemAgent(const QString &text, Agent *agent)
{
    this->agent = agent;
    setText(text);
    setTextAlignment( Qt::AlignCenter );
    setFlags( flags() ^ Qt::ItemIsEditable );
}

TableWidgetItemAgent::~TableWidgetItemAgent() = default;
