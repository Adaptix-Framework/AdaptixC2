#ifndef ADAPTIXCLIENT_SESSIONSGRAPH_H
#define ADAPTIXCLIENT_SESSIONSGRAPH_H

#include <main.h>
#include <Agent/Agent.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/GraphScene.h>

class SessionsGraph final : public QGraphicsView
{
Q_OBJECT
    QWidget* mainWidget = nullptr;

    QVector<GraphItem*> items;
    GraphScene* graphScene = nullptr;
    GraphItem*  rootItem   = nullptr;
    int timerId = 0;

public:
    explicit SessionsGraph( QWidget *parent = nullptr );
    ~SessionsGraph() override;

    GraphScene* GetGraphScene() const { return this->graphScene; }

    void RootInit();
    bool IsRootItem( const GraphItem* item ) const;
    void LinkToRoot( GraphItem* item ) const;

    void AddAgent(Agent* agent, bool drawTree);
    void RemoveAgent(Agent* agent, bool drawTree);
    void RelinkAgent(const Agent* parentAgent, const Agent* childAgent, const QString &linkName, bool drawTree) const;
    void UnlinkAgent(const Agent* parentAgent, const Agent* childAgent, bool drawTree) const;

    void Clear();
    void TreeDraw() const;
    void scaleView(qreal scaleFactor);
    void itemMoved();

protected:
    void wheelEvent( QWheelEvent* event ) override;
    void timerEvent( QTimerEvent* event ) override;
};

#endif //ADAPTIXCLIENT_SESSIONSGRAPH_H
