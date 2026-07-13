#include <Agent/Agent.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/LayoutTreeLeft.h>
#include <UI/Graph/LayoutTreeTop.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Graph/GraphControlPanel.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

SessionsGraph::SessionsGraph(QWidget* parent) : QGraphicsView(parent)
{
    this->mainWidget = parent;

    QString project = static_cast<AdaptixWidget *>(parent)->GetProfile()->GetProject();

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget("Sessions Graph:Dock-" + project, KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Sessions Graph");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon( ":/icons/graph" ), KDDockWidgets::IconPlace::TabBar);

    setDragMode( QGraphicsView::RubberBandDrag );
    setCacheMode( QGraphicsView::CacheNone );
    setViewportUpdateMode( QGraphicsView::FullViewportUpdate );
    setTransformationAnchor( QGraphicsView::AnchorUnderMouse );
    setRenderHint( QPainter::Antialiasing );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );
    this->scaleView( 0.8 );

    {
        QPalette pal = this->palette();
        pal.setColor(QPalette::Active,   QPalette::Highlight, QColor(COLOR_BrightOrange));
        pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor(COLOR_BrightOrange));
        this->setPalette(pal);
        viewport()->setPalette(pal);
    }

    this->graphScene = new GraphScene( 10, this->mainWidget, this );
    this->graphScene->setItemIndexMethod( QGraphicsScene::NoIndex );
    setScene( this->graphScene );

    this->RootInit();

    this->controlPanel = new GraphControlPanel(this, this->viewport());
    this->controlPanel->show();
}

SessionsGraph::~SessionsGraph()
{
    dockWidget = nullptr;
}

void SessionsGraph::RootInit()
{
    this->rootItem = new GraphItem( this, nullptr );
    this->graphScene->addItem( rootItem );

    this->items.push_back( rootItem );
}

bool SessionsGraph::IsRootItem(const GraphItem *item) const
{
    return item == this->rootItem;
}

void SessionsGraph::LinkToRoot(GraphItem *item) const
{
    item->parentLink = new GraphItemLink(this->rootItem, item, "");
    item->parentItem = this->rootItem;
    this->rootItem->AddChild( item );
    this->graphScene->addItem( item->parentLink );
}

void SessionsGraph::AddAgent(Agent *agent, bool drawTree)
{
    GraphItem* item = new GraphItem( this, agent );
    agent->graphItem = item;

    if (agent->parentId == 0)
        this->LinkToRoot(item);

    this->graphScene->addItem( item );
    this->items.push_back( item );

    if (drawTree)
        this->ApplyFiltersAndLayout();
}

void SessionsGraph::RemoveAgent(Agent* agent, bool drawTree)
{
    if ( !agent->graphItem )
        return;

    /// PARENT
    if (agent->graphItem->parentItem)
        agent->graphItem->parentItem->RemoveChild(agent->graphItem);

    /// PARENT LINK
    if (agent->graphItem->parentLink) {
        this->graphScene->removeItem(agent->graphItem->parentLink);
        delete agent->graphItem->parentLink;
        agent->graphItem->parentLink = nullptr;
    }

    /// CHILD
    auto childs = agent->graphItem->childItems;
    if ( !childs.empty() ) {
        for (int i = 0; i < childs.size(); i++) {
            /// childs[i]->agent->graphItem->parentLink = nullptr;
            /// childs[i]->agent->graphItem->parentItem = nullptr;
            this->LinkToRoot(childs[i]->agent->graphItem);
        }
    }
    agent->graphItem->childItems.clear();

    /// CHILD LINK
    auto links = agent->graphItem->childLinks;
    if ( !links.empty() ) {
        for (int i = 0; i < links.size(); i++) {
            if (links[i]) {
                this->graphScene->removeItem(links[i]);
                delete links[i];
                links[i] = nullptr;
            }
        }
    }
    agent->graphItem->childLinks.clear();

    /// NODE
    for ( int i = 0; i < this->items.size(); i++ ) {
        if ( this->items[ i ] == agent->graphItem ) {
            this->items.erase( this->items.begin() + i );
            break;
        }
    }
    this->graphScene->removeItem( agent->graphItem );
    delete agent->graphItem;
    agent->graphItem = nullptr;

    if (drawTree)
        this->ApplyFiltersAndLayout();
}

void SessionsGraph::RelinkAgent(const Agent* parentAgent, const Agent* childAgent, const QString &linkName, const bool drawTree)
{
    if (!parentAgent || !childAgent || !parentAgent->graphItem || !childAgent->graphItem)
        return;

    if (childAgent->graphItem->parentItem) {
        childAgent->graphItem->parentItem->RemoveChild(childAgent->graphItem);
        childAgent->graphItem->parentItem = nullptr;
    }

    if (childAgent->graphItem->parentLink) {
        this->graphScene->removeItem(childAgent->graphItem->parentLink);
        delete childAgent->graphItem->parentLink;
        childAgent->graphItem->parentLink = nullptr;
    }

    childAgent->graphItem->parentLink = new GraphItemLink(parentAgent->graphItem, childAgent->graphItem, linkName);
    childAgent->graphItem->parentItem = parentAgent->graphItem;
    parentAgent->graphItem->AddChild(childAgent->graphItem);

    parentAgent->graphItem->update();
    childAgent->graphItem->update();

    this->graphScene->addItem( childAgent->graphItem->parentLink );

    if (drawTree)
        this->ApplyFiltersAndLayout();
}

void SessionsGraph::UnlinkAgent(const Agent* parentAgent, const Agent* childAgent, bool drawTree)
{
    Q_UNUSED(parentAgent);
    if (!childAgent || !childAgent->graphItem)
        return;

    if (childAgent->graphItem->parentItem) {
        childAgent->graphItem->parentItem->RemoveChild(childAgent->graphItem);
        childAgent->graphItem->parentItem = nullptr;
    }

    if (childAgent->graphItem->parentLink) {
        this->graphScene->removeItem(childAgent->graphItem->parentLink);
        delete childAgent->graphItem->parentLink;
        childAgent->graphItem->parentLink = nullptr;
    }

    this->LinkToRoot(childAgent->graphItem);

    childAgent->graphItem->update();

    if (drawTree)
        this->ApplyFiltersAndLayout();
}

void SessionsGraph::TreeDraw() const
{
    if (layoutDirection == LayoutTopToBottom)
        LayoutTreeTop::draw(this->rootItem);
    else
        LayoutTreeLeft::draw(this->rootItem);

    QRectF totalRect;
    for (const auto* item : this->items) {
        totalRect = totalRect.united(item->sceneBoundingRect());
    }
    if (this->rootItem)
        totalRect = totalRect.united(this->rootItem->sceneBoundingRect());
    if (!totalRect.isNull())
        this->graphScene->setSceneRect(totalRect.adjusted(-50, -50, 200, 200));
}

void SessionsGraph::SetLayoutDirection(GraphLayoutDirection direction)
{
    layoutDirection = direction;
    TreeDraw();
}

void SessionsGraph::UpdateIcons() const
{
    for (int i = 0; i < items.size(); i++)
        items[i]->update();
}

void SessionsGraph::Clear()
{
    for ( int i = 0; i < this->items.size(); i++ ) {
        if (this->items[i]) {
            if (this->items[i]->parentLink) {
                this->graphScene->removeItem(this->items[i]->parentLink);
                delete this->items[i]->parentLink;
                this->items[i]->parentLink = nullptr;
            }
            if (this->items[i]->agent)
                this->items[i]->agent->graphItem = nullptr;
            this->graphScene->removeItem( this->items[ i ] );
            delete this->items[ i ];
            this->items[ i ] = nullptr;
        }
    }
    this->items.clear();

    this->RootInit();
}

void SessionsGraph::scaleView(qreal scaleFactor)
{
    const qreal factor = this->transform().scale( scaleFactor, scaleFactor ).mapRect( QRectF( 0, 0, 1, 1 ) ).width();
    if ( factor < 0.3 || factor > 3 )
        return;

    scale(scaleFactor, scaleFactor);
}

void SessionsGraph::wheelEvent( QWheelEvent* event )
{
    if ( QApplication::keyboardModifiers() & Qt::ShiftModifier ) {
        horizontalScrollBar()->event( event );
    }
    else if ( QApplication::keyboardModifiers() & Qt::ControlModifier ) {
        scaleView( pow( 2., event->angleDelta().y() / 500.0 ) );
        event->accept();
        return;
    }
    else {
        verticalScrollBar()->event( event );
    }
    event->accept();
}

void SessionsGraph::resizeEvent( QResizeEvent* event )
{
    QGraphicsView::resizeEvent( event );
    if (this->controlPanel)
        this->controlPanel->reposition();
}

void SessionsGraph::drawForeground( QPainter* painter, const QRectF& rect )
{
    Q_UNUSED(rect);
    const QRect rb = rubberBandRect();
    if (rb.isNull())
        return;

    painter->save();
    painter->resetTransform();
    QColor line(COLOR_BrightOrange);
    QColor fill(line);
    fill.setAlpha(50);
    painter->setPen(QPen(line, 1));
    painter->setBrush(fill);
    painter->drawRect(rb.adjusted(0, 0, -1, -1));
    painter->restore();
}

static bool agentIsActive(const Agent* agent)
{
    if (!agent)
        return true;
    const QString& m = agent->data.Mark;
    return m != "Terminated" && m != "Inactive" && m != "Disconnect" && m != "Unlink";
}

static void applyVisibility(GraphItem* item, const SessionsGraph* view, bool hideAncestral)
{
    if (!item)
        return;

    bool hide = hideAncestral;
    if (!hide && item->agent) {
        if (view->GetFilterActiveOnlyActive() && !agentIsActive(item->agent))
            hide = true;
        else if (view->GetFilterWithChildOnly() && item->agent->childsId.isEmpty())
            hide = true;
    }

    item->setVisible(!hide);
    if (item->note)
        item->note->setVisible(!hide);
    if (item->parentLink)
        item->parentLink->setVisible(!hide && !(item->parentItem && !item->parentItem->isVisible()));

    for (GraphItem* child : item->childItems)
        applyVisibility(child, view, hide);
}

void SessionsGraph::ApplyFiltersAndLayout()
{
    applyVisibility(this->rootItem, this, /*hideAncestral=*/false);
    this->TreeDraw();

    this->graphScene->invalidate(this->graphScene->sceneRect(), QGraphicsScene::AllLayers);
    if (auto* vp = this->viewport())
        vp->update();
}

void SessionsGraph::SetFilterActiveOnly(bool on)
{
    filterActiveOnly = on;
    ApplyFiltersAndLayout();
}

void SessionsGraph::SetFilterWithChildOnly(bool on)
{
    filterWithChildOnly = on;
    ApplyFiltersAndLayout();
}

void SessionsGraph::SetNoteField(int field, bool on)
{
    switch (field) {
        case 0: noteFields.id       = on; break;
        case 1: noteFields.name     = on; break;
        case 2: noteFields.pid      = on; break;
        case 3: noteFields.user     = on; break;
        case 4: noteFields.computer = on; break;
        default: return;
    }
    RefreshAllNotes();
}

void SessionsGraph::SetNoteFields(const GraphNoteFields& f)
{
    noteFields = f;
    RefreshAllNotes();
}

void SessionsGraph::RefreshAllNotes() const
{
    for (GraphItem* item : this->items)
        if (item && item->agent)
            item->refreshNoteContent();
}
