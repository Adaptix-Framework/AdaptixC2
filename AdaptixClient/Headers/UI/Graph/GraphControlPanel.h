#ifndef ADAPTIXCLIENT_GRAPHCONTROLPANEL_H
#define ADAPTIXCLIENT_GRAPHCONTROLPANEL_H

#include <main.h>
#include <UI/Graph/GraphItem.h>

class SessionsGraph;

namespace oclero::qlementine { class SegmentedControl; }
namespace oclero::qlementine { class Switch; }
class QToolButton;
class QWidget;

class GraphControlPanel final : public QFrame
{
    Q_OBJECT

    SessionsGraph* graph = nullptr;
    bool expanded = false;

    QToolButton* toggleBtn = nullptr;
    QWidget* contentWidget = nullptr;
    oclero::qlementine::SegmentedControl* layoutSegment = nullptr;
    oclero::qlementine::Switch* activeOnlySwitch = nullptr;
    oclero::qlementine::Switch* withChildSwitch = nullptr;
    oclero::qlementine::Switch* noteIdSwitch = nullptr;
    oclero::qlementine::Switch* noteNameSwitch = nullptr;
    oclero::qlementine::Switch* notePidSwitch = nullptr;
    oclero::qlementine::Switch* noteUserSwitch = nullptr;
    oclero::qlementine::Switch* noteComputerSwitch = nullptr;

public:
    explicit GraphControlPanel(SessionsGraph* graph, QWidget* parent = nullptr);

    void reposition();

private:
    void buildCollapsed();
    void buildExpanded();
    void setExpanded(bool on);
    void syncFromGraph();
};

#endif
