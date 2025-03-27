#ifndef ADAPTIXCLIENT_GRAPHITEMLINK_H
#define ADAPTIXCLIENT_GRAPHITEMLINK_H

#include <main.h>

class Agent;
class SessionsGraph;
class GraphItem;

class GraphItemLink final : public QGraphicsItem
{
    GraphItem* src = nullptr;
    GraphItem* dst = nullptr;
    QColor  color;
    QString listenerName;
    QString listenerType;
    QString linkName;
    QString status;
    QPointF srcPoint;
    QPointF dstPoint;

public:
    explicit GraphItemLink(GraphItem* src, GraphItem* dst, QString linkName);
    ~GraphItemLink() override;

    void adjust();

protected:
    QRectF boundingRect() const override;
    void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget ) override;

    static void paintLineText( QPainter* painter, double angle, const QLineF &line, const QString &text, QColor textColor );
};

#endif //ADAPTIXCLIENT_GRAPHITEMLINK_H
