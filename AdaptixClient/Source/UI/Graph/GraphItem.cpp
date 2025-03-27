#include <UI/Graph/GraphItem.h>
#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/SessionsGraph.h>

GraphItemNote::GraphItemNote(const QString &h, const QString &t)
{
    this->header = h;
    this->text = t;
}

GraphItemNote::~GraphItemNote() = default;

QRectF GraphItemNote::boundingRect() const
{
    qreal width = this->text.length();
    if (this->text.length() < this->header.length())
        width = this->header.length();

    return QRectF( 0, 0, width * 8, 50 );
}

void GraphItemNote::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
     painter->setPen( COLOR_White );
     painter->drawText( this->boundingRect(), Qt::AlignCenter | Qt::AlignTop,    this->header);
     painter->drawText( this->boundingRect(), Qt::AlignCenter | Qt::AlignBottom, this->text);
}





GraphItem::GraphItem( SessionsGraph* graphView, Agent* agent )
{
    this->sessionsGraph = graphView;
    this->agent         = agent;
    this->rect          = QRectF( 0, 0, 100, 100 );

    this->setZValue( -1 );
    this->setCacheMode( QGraphicsItem::DeviceCoordinateCache );
    this->setFlag( QGraphicsItem::ItemIsMovable );
    this->setFlag( QGraphicsItem::ItemSendsGeometryChanges );
    this->setFlag( QGraphicsItem::ItemIsSelectable );

    if ( agent ) {
        const QString note1 = QString("%1 @ %2").arg( agent->data.Username ).arg( agent->data.Computer );
        const QString note2 = QString("%1 (%2)").arg( agent->data.Id ).arg( agent->data.Process );
        this->note = new GraphItemNote( note1, note2 );
        sessionsGraph->scene()->addItem( this->note );
    }
}

GraphItem::~GraphItem()
{
    if (this->note) {
        this->sessionsGraph->GetGraphScene()->removeItem(this->note);
        delete this->note;
        this->note = nullptr;
    }
};

QRectF GraphItem::boundingRect() const
{
    return this->rect;
}

void GraphItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    if ( this->sessionsGraph->IsRootItem(this) )
        painter->drawImage(rect, QImage(":/graph/firewall"));
    else
        painter->drawImage(rect, this->agent->graphImage);

    if ( this->isSelected() ) {
        painter->setPen( QPen( QBrush( COLOR_BrightOrange ), 1, Qt::DotLine ) );
        painter->drawRect( boundingRect() );
    }
}

void GraphItem::AddChild(GraphItem *item)
{
    this->childItems.push_back( item );
}

void GraphItem::RemoveChild(const GraphItem* item )
{
    for ( int i = 0; i < this->childItems.size(); i++ ) {
        if ( this->childItems[ i ] == item ) {
            this->childItems.erase( this->childItems.begin() + i );
            break;
        }
    }

    for ( int i = 0; i < this->childLinks.size(); i++ ) {
        if ( this->childLinks[ i ] == item->parentLink ) {
            this->childLinks.erase( this->childLinks.begin() + i );
            break;
        }
    }
}

void GraphItem::AddLink( GraphItemLink* link )
{
    childLinks.push_back( link );
}

void GraphItem::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
    update();
    this->sessionsGraph->itemMoved();
    QGraphicsItem::mousePressEvent( event );
}

void GraphItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
    update();
    this->sessionsGraph->itemMoved();
    QGraphicsItem::mouseReleaseEvent( event );
}

void GraphItem::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    update();
    this->sessionsGraph->itemMoved();
    QGraphicsItem::mouseMoveEvent( event );
}

void GraphItem::adjust()
{
    for ( auto link : this->childLinks )
        link->adjust();
}

void GraphItem::calculateForces()
{
    if ( !this->scene() || this->scene()->mouseGrabberItem() == this ) {
        this->point = this->pos();
        return;
    }

    QPointF newPos   = this->pos();
    QRectF sceneRect = this->scene()->sceneRect();

    qreal leftBound   = sceneRect.left() + 10;
    qreal rightBound  = sceneRect.right() - 10;
    qreal topBound    = sceneRect.top() + 10;
    qreal bottomBound = sceneRect.bottom() - 10;

    this->point.setX( std::clamp(newPos.x(), leftBound, rightBound) );
    this->point.setY( std::clamp(newPos.y(), topBound, bottomBound) );
}

bool GraphItem::advancePosition()
{
    if ( this->point == this->pos() )
        return false;

    setPos( this->point );
    return true;
}

QVariant GraphItem::itemChange( GraphicsItemChange change, const QVariant& value )
{
    if ( change == ItemPositionChange ) {
        this->adjust();
        this->sessionsGraph->itemMoved();

        if ( this->note ) {
            QRectF  rect     = this->boundingRect();
            QRectF  noteRect = this->note->boundingRect();
            QPointF pos      = value.toPointF();
            QPointF posRect  = QPointF( pos.x() + (rect.width() - noteRect.width()) / 2, pos.y() + rect.height() - 10);

            this->note->setPos(posRect);
        }
    }

    return QGraphicsItem::itemChange( change, value );
}