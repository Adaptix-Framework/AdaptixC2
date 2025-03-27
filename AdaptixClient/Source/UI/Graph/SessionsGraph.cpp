#include <UI/Graph/SessionsGraph.h>
#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/LayoutTreeLeft.h>

SessionsGraph::SessionsGraph(QWidget* parent) : QGraphicsView(parent)
{
    this->mainWidget = parent;

    setDragMode( QGraphicsView::RubberBandDrag );
    setCacheMode( QGraphicsView::CacheBackground );
    setViewportUpdateMode( QGraphicsView::BoundingRectViewportUpdate );
    setTransformationAnchor( QGraphicsView::AnchorUnderMouse );
    setRenderHint( QPainter::Antialiasing );
    this->scaleView( 0.8 );

    this->graphScene = new GraphScene( 10, this->mainWidget, this );
    this->graphScene->setItemIndexMethod( QGraphicsScene::BspTreeIndex );
    setScene( this->graphScene );

    this->RootInit();
}

SessionsGraph::~SessionsGraph() = default;

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
}

void SessionsGraph::AddAgent(Agent *agent, bool drawTree)
{
    if (agent->data.Mark == "Terminated")
        return;

    GraphItem* item = new GraphItem( this, agent );
    agent->graphItem = item;

    if (agent->parentId.isEmpty())
        this->LinkToRoot(item);

    this->graphScene->addItem( item );
    this->graphScene->addItem( item->parentLink );
    this->items.push_back( item );

    if (drawTree)
        this->TreeDraw();
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

    ///CHILD
    auto childs = agent->graphItem->childItems;
    if ( !childs.empty() ) {
        for (int i = 0; i < childs.size(); i++) {
            // childs[i]->agent->graphItem->parentLink = nullptr;
            // childs[i]->agent->graphItem->parentItem = nullptr;
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

    // NODE
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
        this->TreeDraw();
}

void SessionsGraph::RelinkAgent(const Agent* parentAgent, const Agent* childAgent, const QString &linkName, const bool drawTree) const
{
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
        this->TreeDraw();
}

void SessionsGraph::UnlinkAgent(const Agent* parentAgent, const Agent* childAgent, bool drawTree) const
{
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
    this->graphScene->addItem( childAgent->graphItem->parentLink );

    childAgent->graphItem->update();

    if (drawTree)
        this->TreeDraw();
}

void SessionsGraph::TreeDraw() const
{
    LayoutTreeLeft::draw(this->rootItem);
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

void SessionsGraph::itemMoved()
{
    if ( !timerId )
        timerId = this->startTimer( 40 );
}

void SessionsGraph::timerEvent( QTimerEvent* event )
{
    bool itemsMoved = false;

    for ( auto item : this->items ) {
        item->calculateForces();
        itemsMoved = item->advancePosition();
        item->adjust();
    }

    if ( !itemsMoved ) {
        killTimer( timerId );
        timerId = 0;
    }
}

void SessionsGraph::wheelEvent( QWheelEvent* event )
{
    if ( QApplication::keyboardModifiers() & Qt::ShiftModifier )
        horizontalScrollBar()->event( event );
    else if ( QApplication::keyboardModifiers() & Qt::ControlModifier )
        scaleView( pow( 2., event->angleDelta().y() / 500.0 ) );
    else
        verticalScrollBar()->event( event );

    event->ignore();
}
