#include <UI/Graph/GraphControlPanel.h>
#include <UI/Graph/SessionsGraph.h>
#include <Client/Settings.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/SegmentedControl.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>
#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace {
constexpr int kMargin = 8;
}

static QString rgba(const QColor& c, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

GraphControlPanel::GraphControlPanel(SessionsGraph* graph, QWidget* parent) : QFrame(parent), graph(graph)
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    const QString panelBg  = rgba(t.backgroundColorMain4, 220);
    const QString panelBrd = rgba(t.borderColor, 120);
    const QString labelFg  = t.secondaryColor.name();
    const QString btnBg    = rgba(t.neutralColor, 80);
    const QString btnBgHov = rgba(t.neutralColorHovered, 120);

    setAttribute(Qt::WA_NoSystemBackground, false);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "GraphControlPanel { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel { color: %3; }"
        "QToolButton { background: %4; border: none; border-radius: 6px; padding: 4px; }"
        "QToolButton:hover { background: %5; }"
    ).arg(panelBg, panelBrd, labelFg, btnBg, btnBgHov));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    buildCollapsed();

    reposition();
}

void GraphControlPanel::buildCollapsed()
{
    toggleBtn = new QToolButton(this);
    toggleBtn->setText(QStringLiteral("\u2699"));
    toggleBtn->setFixedSize(28, 28);
    toggleBtn->setCursor(Qt::PointingHandCursor);
    toggleBtn->setToolTip(tr("Graph controls"));
    connect(toggleBtn, &QToolButton::clicked, this, [this]() { setExpanded(!expanded); });

    if (auto* l = qobject_cast<QVBoxLayout*>(layout())) {
        auto* headerRow = new QHBoxLayout();
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(6);
        headerRow->addWidget(toggleBtn);
        headerRow->addStretch();
        l->addLayout(headerRow);
    }

    buildExpanded();
    contentWidget->setVisible(false);
    adjustSize();
}

void GraphControlPanel::buildExpanded()
{
    contentWidget = new QWidget(this);
    auto* v = new QVBoxLayout(contentWidget);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    auto* layoutLabel = new QLabel(tr("Layout"), contentWidget);
    layoutSegment = new oclero::qlementine::SegmentedControl(contentWidget);
    layoutSegment->addItem(tr("Left \u2192 Right"));
    layoutSegment->addItem(tr("Top \u2192 Bottom"));
    layoutSegment->setFixedHeight(26);
    connect(layoutSegment, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this]() {
        if (graph) {
            const int idx = layoutSegment->currentIndex();
            graph->SetLayoutDirection(idx == 0 ? LayoutLeftToRight : LayoutTopToBottom);
        }
    });
    v->addWidget(layoutLabel);
    v->addWidget(layoutSegment);

    auto* filterLabel = new QLabel(tr("Filters"), contentWidget);
    v->addWidget(filterLabel);

    activeOnlySwitch = new oclero::qlementine::Switch(contentWidget);
    activeOnlySwitch->setText(tr("Active only"));
    connect(activeOnlySwitch, &oclero::qlementine::Switch::toggled, this, [this](bool on) {
        if (graph) graph->SetFilterActiveOnly(on);
    });
    v->addWidget(activeOnlySwitch);

    withChildSwitch = new oclero::qlementine::Switch(contentWidget);
    withChildSwitch->setText(tr("With child only"));
    connect(withChildSwitch, &oclero::qlementine::Switch::toggled, this, [this](bool on) {
        if (graph) graph->SetFilterWithChildOnly(on);
    });
    v->addWidget(withChildSwitch);

    if (GlobalClient && GlobalClient->settings) {
        activeOnlySwitch->setChecked(GlobalClient->settings->data.GraphAutoHideInactive);
        withChildSwitch->setChecked(GlobalClient->settings->data.GraphAutoHideNoChilds);
    }

    auto* noteLabel = new QLabel(tr("Note fields"), contentWidget);
    v->addWidget(noteLabel);

    auto addNoteSwitch = [this, v](const QString& title, int fieldIdx) {
        auto* sw = new oclero::qlementine::Switch(contentWidget);
        sw->setText(title);
        sw->setChecked(true);
        connect(sw, &oclero::qlementine::Switch::toggled, this, [this, fieldIdx](bool on) {
            if (graph) graph->SetNoteField(fieldIdx, on);
        });
        v->addWidget(sw);
        return sw;
    };
    noteIdSwitch       = addNoteSwitch(tr("ID"),       0);
    noteNameSwitch     = addNoteSwitch(tr("Name"),     1);
    notePidSwitch      = addNoteSwitch(tr("PID"),      2);
    noteUserSwitch     = addNoteSwitch(tr("User"),     3);
    noteComputerSwitch = addNoteSwitch(tr("Computer"), 4);

    if (auto* l = qobject_cast<QVBoxLayout*>(layout()))
        l->addWidget(contentWidget);

    syncFromGraph();
}

void GraphControlPanel::syncFromGraph()
{
    if (!graph)
        return;

    QSignalBlocker blockLayout(layoutSegment);
    QSignalBlocker blockActive(activeOnlySwitch);
    QSignalBlocker blockChild(withChildSwitch);
    QSignalBlocker blockId(noteIdSwitch);
    QSignalBlocker blockName(noteNameSwitch);
    QSignalBlocker blockPid(notePidSwitch);
    QSignalBlocker blockUser(noteUserSwitch);
    QSignalBlocker blockComputer(noteComputerSwitch);

    layoutSegment->setCurrentIndex(graph->GetLayoutDirection() == LayoutLeftToRight ? 0 : 1);
    activeOnlySwitch->setChecked(graph->GetFilterActiveOnlyActive());
    withChildSwitch->setChecked(graph->GetFilterWithChildOnly());

    const GraphNoteFields& nf = graph->GetNoteFields();
    noteIdSwitch->setChecked(nf.id);
    noteNameSwitch->setChecked(nf.name);
    notePidSwitch->setChecked(nf.pid);
    noteUserSwitch->setChecked(nf.user);
    noteComputerSwitch->setChecked(nf.computer);
}

void GraphControlPanel::setExpanded(bool on)
{
    expanded = on;
    toggleBtn->setText(on ? QStringLiteral("\u25B2") : QStringLiteral("\u2699"));
    if (contentWidget)
        contentWidget->setVisible(on);
    adjustSize();
    reposition();
}

void GraphControlPanel::reposition()
{
    move(kMargin, kMargin);
    raise();
}
