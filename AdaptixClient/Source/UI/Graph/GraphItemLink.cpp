#include <UI/Graph/GraphItemLink.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/SessionsGraph.h>

GraphItemLink::GraphItemLink( GraphItem* src, GraphItem* dst, QString linkName)
{
    this->src = src;
    this->dst = dst;
    this->listenerName = dst->agent->data.Listener;
    this->listenerType = dst->agent->connType;
    this->linkName     = linkName;

    if (listenerType == "external") {
        this->color = QColor(COLOR_KellyGreen);
    }
    else if (listenerType == "internal") {
        this->color = QColor(COLOR_PastelYellow);
    }

    setAcceptedMouseButtons( Qt::NoButton );

    src->AddLink(this);

    this->adjust();
}

GraphItemLink::~GraphItemLink() = default;

void GraphItemLink::adjust()
{
    if ( !this->src || !this->dst )
        return;

    QPointF srcLine = mapFromItem( this->src, 50, 60 );
    QPointF dstLine = mapFromItem( this->dst, 50, 40 );

    QLineF line(srcLine, dstLine);
    double lineLength = line.length();

    this->prepareGeometryChange();

    if ( lineLength > 20.0 ) {
        auto edgeSpace  = 65;
        auto edgeOffset = QPointF( ( ( line.dx() * edgeSpace ) / lineLength ), ( ( line.dy() * edgeSpace ) / lineLength ) - 10 );

        this->srcPoint = line.p1() + edgeOffset;
        this->dstPoint = line.p2() - edgeOffset;
    } else {
        this->srcPoint = line.p1();
        this->dstPoint = line.p2();
    }
}

QRectF GraphItemLink::boundingRect() const
{
    if ( !this->src || !this->dst )
        return QRectF();

    double arrowSize = 10.0;
    double width = 1.0;
    double extra = (width + arrowSize) / 2.0;

    QSizeF rectSize = QSizeF(this->dstPoint.x() - this->srcPoint.x(),  this->dstPoint.y() - this->srcPoint.y());

    return QRectF(this->srcPoint, rectSize).normalized().adjusted( -extra, -extra, extra, extra );
}

void GraphItemLink::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    if ( !this->src || !this->dst )
        return;

    QLineF line = QLineF( this->srcPoint, this->dstPoint );
    if ( qFuzzyCompare(line.length(), static_cast<qreal>(0.)) )
        return;

    double arrowSize = 10.0;
    auto angle         = std::atan2( -line.dy(), line.dx() );
    auto sourceArrowP1 = this->srcPoint + QPointF( sin( angle + M_PI / 3 ) * arrowSize, cos( angle + M_PI / 3 ) * arrowSize );
    auto sourceArrowP2 = this->srcPoint + QPointF( sin( angle + M_PI - M_PI / 3 ) * arrowSize, cos( angle + M_PI - M_PI / 3 ) * arrowSize );
    auto destArrowP1   = this->dstPoint + QPointF( sin( angle - M_PI / 3 ) * arrowSize, cos( angle - M_PI / 3 ) * arrowSize );
    auto destArrowP2   = this->dstPoint + QPointF( sin( angle - M_PI + M_PI / 3 ) * arrowSize, cos( angle - M_PI + M_PI / 3 ) * arrowSize );

    if ( (this->src->agent == nullptr) && this->dst->agent )
    {
        if (this->listenerType == "external") {
            painter->setPen( QPen( this->color, 3, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin ) );
            painter->setBrush( this->color );
            painter->drawPolygon( QPolygonF() << line.p1() << sourceArrowP1 << sourceArrowP2 );

            paintLineText(painter, angle, line, this->listenerName, this->color);
        }
        else if (this->listenerType == "internal") {
            QColor greyColor(COLOR_SaturGray);

            painter->setPen( QPen( greyColor, 3, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin ) );
            painter->setBrush( greyColor );

            QString linkText = QString("UNLINK [%1]").arg(this->listenerName);;
            paintLineText(painter, angle, line, linkText, greyColor);
        }
    }
    else {
        painter->setPen( QPen( this->color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
        painter->setBrush( this->color  );
        painter->drawPolygon( QPolygonF() << line.p2() << destArrowP1 << destArrowP2 );

        QString linkText = this->listenerName;
        if (!this->linkName.isEmpty())
            linkText = QString("%1 (%2)").arg(this->listenerName).arg(this->linkName);

        paintLineText(painter, angle, line, linkText, this->color);
    }
}

void GraphItemLink::paintLineText( QPainter* painter, const double angle, const QLineF &line, const QString &text, const QColor textColor )
{
    if (text.isEmpty()) {
        painter->drawLine( line );
        return;
    }

    auto font = painter->font();
    font.setPointSizeF( font.pointSizeF() * 1.1 );
    painter->setFont( font );

    const auto metrics = QFontMetrics( painter->font() );
    const auto textPos = QPointF( -metrics.horizontalAdvance( text ) / 2, metrics.height() / 2 - 6 );

    int    gapLength      = metrics.horizontalAdvance( text ) + 14;
    QLineF lineBeforeText = QLineF( line.p1(), line.pointAt( ( line.length() - gapLength ) / ( 2 * line.length() ) ) );
    QLineF lineAfterText  = QLineF( line.pointAt( 1 - ( line.length() - gapLength ) / ( 2 * line.length() ) ), line.p2() );

    painter->drawLine( lineBeforeText );
    painter->drawLine( lineAfterText  );

    painter->save();

    painter->translate( line.pointAt( 0.5 ) );

    auto angleDegrees = ( -angle * 180 / M_PI );

    if ( angleDegrees > 90 || angleDegrees < -90 )
        angleDegrees += 180;

    painter->rotate(angleDegrees);

    painter->setPen( QPen(textColor) );
    painter->drawText(textPos, text);

    painter->restore();
}