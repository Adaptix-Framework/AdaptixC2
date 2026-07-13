#include <Agent/Agent.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Widgets/AdaptixWidget.h>

namespace {
    constexpr qreal kNodeSize   = 100.0;

    constexpr qreal kNotePadX   = 6.0;
    constexpr qreal kNoteHeight = 50.0;

    constexpr qreal kBadgeW        = 42.0;
    constexpr qreal kBadgeH        = 24.0;
    constexpr qreal kBadgeOffsetX  = 38.0;
    constexpr qreal kBadgeOffsetY  = -6.0;
    constexpr int   kBadgeFontSize = 11;
    const QColor    kBadgeColor    = QColor(0, 200, 0);

    QRectF badgeRect(const QRectF& nodeRect)
    {
        return QRectF(nodeRect.right() - kBadgeOffsetX, nodeRect.top() + kBadgeOffsetY, kBadgeW, kBadgeH);
    }
}

GraphItemNote::GraphItemNote(const QString &h, const QString &t)
{
    this->header = h;
    this->text = t;
    this->setAcceptedMouseButtons( Qt::NoButton );
    this->setAcceptHoverEvents( false );
    this->setFlag( QGraphicsItem::ItemIsPanel, false );
}

GraphItemNote::~GraphItemNote() = default;

void GraphItemNote::setHeader(const QString &h)
{
    if (header == h)
        return;
    prepareGeometryChange();
    header = h;
    update();
}

void GraphItemNote::setText(const QString &t)
{
    if (text == t)
        return;
    prepareGeometryChange();
    text = t;
    update();
}

QRectF GraphItemNote::boundingRect() const
{
    const QFontMetrics fm{QFont{}};
    const qreal w = std::max(fm.horizontalAdvance(this->header), fm.horizontalAdvance(this->text));
    return QRectF(0, 0, w + kNotePadX * 2, kNoteHeight);
}

void GraphItemNote::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    painter->setPen( COLOR_White );
    if (!this->header.isEmpty())
        painter->drawText( this->boundingRect(), Qt::AlignCenter | Qt::AlignTop,    this->header);
    painter->drawText( this->boundingRect(), Qt::AlignCenter | Qt::AlignBottom, this->text);
}





GraphItem::GraphItem( SessionsGraph* graphView, Agent* agent )
{
    this->sessionsGraph = graphView;
    this->agent         = agent;
    this->rect          = QRectF( 0, 0, 100, 100 );

    this->setZValue( -1 );
    this->setCacheMode( QGraphicsItem::NoCache );
    this->setFlag( QGraphicsItem::ItemIsMovable );
    this->setFlag( QGraphicsItem::ItemSendsGeometryChanges );
    this->setFlag( QGraphicsItem::ItemIsSelectable );

    if ( agent ) {
        const GraphNoteFields& nf = sessionsGraph->GetNoteFields();
        QString noteHeader, noteText;
        buildNoteTexts(nf, noteHeader, noteText);
        this->note = new GraphItemNote( noteHeader, noteText );
        sessionsGraph->scene()->addItem( this->note );
    }
}

void GraphItem::buildNoteTexts(const GraphNoteFields& nf, QString& header, QString& text) const
{
    if (!this->agent)
        return;

    if (nf.user || nf.computer) {
        QString user = nf.user ? agent->data.Username : QString();
        QString host = nf.computer ? agent->data.Computer : QString();
        if (user.isEmpty() && host.isEmpty()) {
            header = QString();
        } else if (user.isEmpty()) {
            header = host;
        } else if (host.isEmpty()) {
            header = user;
        } else {
            header = QString("%1 @ %2").arg(user).arg(host);
        }
    } else {
        header = QString();
    }

    QStringList parts;
    if (nf.id)
        parts.append(QString::number(agent->data.Id));
    if (nf.name)
        parts.append(QString("(%1)").arg(agent->data.Name));
    if (nf.pid)
        parts.append(QString(": %1").arg(agent->data.Pid));
    text = parts.join(' ').simplified();
}

void GraphItem::refreshNoteContent()
{
    if (!this->note || !this->agent || !this->sessionsGraph)
        return;
    const GraphNoteFields& nf = sessionsGraph->GetNoteFields();
    QString header, text;
    buildNoteTexts(nf, header, text);
    this->note->setHeader(header);
    this->note->setText(text);
}

void GraphItem::UpdateNote()
{
    refreshNoteContent();
}

GraphItem::~GraphItem()
{
    if (this->note && this->note->scene()) {
        this->sessionsGraph->GetGraphScene()->removeItem(this->note);
        delete this->note;
        this->note = nullptr;
    }
};

QRectF GraphItem::boundingRect() const
{
    QRectF br = this->rect;
    if (HasTunnel())
        br |= ::badgeRect(this->rect);
    return br;
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
        const QRectF br = ::badgeRect(rect);
        painter->setBrush(kBadgeColor);
        painter->setPen(QPen(QColor(0, 0, 0), 2));
        painter->drawRoundedRect(br, 10, 10);
        painter->setPen(QColor(0, 0, 0));
        painter->setFont(QFont("Arial", kBadgeFontSize, QFont::Bold));
        const QString label = (GetTunnelType() == TunnelMarkServer) ? "TunS" : "TunC";
        painter->drawText(br, Qt::AlignCenter, label);
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

void GraphItem::invalidateCache()
{
    update();
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