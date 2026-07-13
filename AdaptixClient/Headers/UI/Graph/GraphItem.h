#ifndef ADAPTIXCLIENT_GRAPHITEM_H
#define ADAPTIXCLIENT_GRAPHITEM_H

#include <main.h>

class Agent;
class SessionsGraph;
class GraphItemLink;

struct GraphNoteFields {
    bool id       = true;
    bool name     = true;
    bool pid      = true;
    bool user     = true;
    bool computer = true;
};

class GraphItemNote final : public QGraphicsItem
{
    QString header;
    QString text;

public:
    explicit GraphItemNote(const QString &h, const QString &t );
    ~GraphItemNote() override;

    void setHeader(const QString &h);
    void setText(const QString &t);

    QRectF boundingRect() const override;
    void   paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget ) override;
};





enum TunnelMarkType {
    TunnelMarkNone   = 0,
    TunnelMarkServer = 1,
    TunnelMarkClient = 2
};

class GraphItem : public QGraphicsItem
{
    SessionsGraph* sessionsGraph = nullptr;
    QRectF  rect;
    int serverTunnelCount = 0;
    int clientTunnelCount = 0;

public:
    GraphItem*     parentItem = nullptr;
    GraphItemLink* parentLink = nullptr;
    GraphItemNote* note       = nullptr;

    QVector<GraphItem*>     childItems;
    QVector<GraphItemLink*> childLinks;

    GraphItem* Thread   = nullptr;
    GraphItem* Ancestor = this;
    double Prelim   = 0;
    double Modifier = 0;
    double Shift    = 0;
    double Change   = 0;

    Agent* agent = nullptr;

    explicit GraphItem( SessionsGraph* graphView, Agent* agent );
    ~GraphItem() override;

    void RemoveChild(const GraphItem* item );

    void AddChild( GraphItem *item );
    void AddLink( GraphItemLink* link );

    void adjust();

    void UpdateNote();
    void refreshNoteContent();

    void AddTunnel(TunnelMarkType type);
    void RemoveTunnel(TunnelMarkType type);
    bool HasTunnel() const { return (serverTunnelCount + clientTunnelCount) > 0; }
    TunnelMarkType GetTunnelType() const { return serverTunnelCount > 0 ? TunnelMarkServer : (clientTunnelCount > 0 ? TunnelMarkClient : TunnelMarkNone); }

    void invalidateCache();

private:
    void buildNoteTexts(const GraphNoteFields& nf, QString& header, QString& text) const;

protected:
    QRectF boundingRect() const override;
    void   paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget ) override;

    QVariant itemChange( GraphicsItemChange change, const QVariant& value ) override;

    void mousePressEvent( QGraphicsSceneMouseEvent* event ) override;
    void mouseReleaseEvent( QGraphicsSceneMouseEvent* event ) override;
    void mouseMoveEvent( QGraphicsSceneMouseEvent* event ) override;
};

#endif
