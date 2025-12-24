#include <Agent/Agent.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Widgets/AdaptixWidget.h>

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
    this->setCacheMode( QGraphicsItem::ItemCoordinateCache );
    this->setFlag( QGraphicsItem::ItemIsMovable );
    this->setFlag( QGraphicsItem::ItemSendsGeometryChanges );
    this->setFlag( QGraphicsItem::ItemIsSelectable );

    if ( agent ) {
        const QString note1 = QString("%1 @ %2").arg( agent->data.Username ).arg( agent->data.Computer );
        const QString note2 = QString("%1 (%2) : %3").arg( agent->data.Id ).arg( agent->data.Name ).arg( agent->data.Pid );
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

static QImage s_firewallImage;

void GraphItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    if ( this->sessionsGraph->IsRootItem(this) ) {
        if (s_firewallImage.isNull())
            s_firewallImage = QImage(":/graph/v1/firewall");
        painter->drawImage(rect, s_firewallImage);
    }
    else
        painter->drawImage(rect, this->agent->graphImage);

    if (HasTunnel()) {
        painter->save();
        QRectF badgeRect(rect.right() - 38, rect.top() - 6, 42, 24);
        painter->setBrush(QColor(0, 200, 0));
        painter->setPen(QPen(QColor(0, 0, 0), 2));
        painter->drawRoundedRect(badgeRect, 10, 10);
        painter->setPen(QColor(0, 0, 0));
        painter->setFont(QFont("Arial", 11, QFont::Bold));
        QString label = (GetTunnelType() == TunnelMarkServer) ? "TunS" : "TunC";
        painter->drawText(badgeRect, Qt::AlignCenter, label);
        painter->restore();
    }

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
    if (event->button() == Qt::MiddleButton && this->agent && this->agent->Console) {
        this->agent->adaptixWidget->LoadConsoleUI(this->agent->data.Id);
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent( event );
}

void GraphItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
    QGraphicsItem::mouseReleaseEvent( event );
}

void GraphItem::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    QGraphicsItem::mouseMoveEvent( event );
}

void GraphItem::adjust()
{
    if ( this->parentLink )
        this->parentLink->adjust();

    for ( auto link : this->childLinks )
        link->adjust();
}

void GraphItem::calculateForces()
{
    this->point = this->pos();
}

void GraphItem::AddTunnel(TunnelMarkType type)
{
    if (type == TunnelMarkServer)
        serverTunnelCount++;
    else if (type == TunnelMarkClient)
        clientTunnelCount++;
    update();
}

void GraphItem::RemoveTunnel(TunnelMarkType type)
{
    if (type == TunnelMarkServer && serverTunnelCount > 0)
        serverTunnelCount--;
    else if (type == TunnelMarkClient && clientTunnelCount > 0)
        clientTunnelCount--;
    update();
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
        if ( this->note ) {
            QRectF  rect     = this->boundingRect();
            QRectF  noteRect = this->note->boundingRect();
            QPointF pos      = value.toPointF();
            QPointF posRect  = QPointF( pos.x() + (rect.width() - noteRect.width()) / 2, pos.y() + rect.height() - 10);

            this->note->setPos(posRect);
        }
    }
    else if ( change == ItemPositionHasChanged ) {
        this->adjust();
    }

    return QGraphicsItem::itemChange( change, value );
}