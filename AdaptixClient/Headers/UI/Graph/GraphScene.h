#ifndef ADAPTIXCLIENT_GRAPHSCENE_H
#define ADAPTIXCLIENT_GRAPHSCENE_H

#include <main.h>

class GraphScene : public QGraphicsScene
{
Q_OBJECT

    QWidget* mainWidget = nullptr;

    int gridSize = 50;

public:
    explicit GraphScene( int gridSize, QWidget* m, QObject* parent = nullptr );
    ~GraphScene() override;

private:
    void mouseMoveEvent( QGraphicsSceneMouseEvent* event ) override;
    void contextMenuEvent( QGraphicsSceneContextMenuEvent *event ) override;
};

#endif //ADAPTIXCLIENT_GRAPHSCENE_H
