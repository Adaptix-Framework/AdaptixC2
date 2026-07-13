#ifndef ADAPTIXCLIENT_SESSIONSGRAPH_H
#define ADAPTIXCLIENT_SESSIONSGRAPH_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Graph/GraphItem.h>

enum GraphLayoutDirection {
    LayoutLeftToRight,
    LayoutTopToBottom
};

class Agent;
class GraphItem;
class GraphScene;
class GraphControlPanel;

class SessionsGraph final : public QGraphicsView
{
Q_OBJECT
    QWidget* mainWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    QVector<GraphItem*> items;
    GraphScene* graphScene = nullptr;
    GraphItem*  rootItem   = nullptr;
    GraphLayoutDirection layoutDirection = LayoutLeftToRight;

    GraphControlPanel* controlPanel = nullptr;

    bool filterActiveOnly    = true;
    bool filterWithChildOnly = false;
    GraphNoteFields noteFields;

public:
    explicit SessionsGraph( QWidget *parent = nullptr );
    ~SessionsGraph() override;

    GraphScene* GetGraphScene() const { return this->graphScene; }
    KDDockWidgets::QtWidgets::DockWidget* dock() { return this->dockWidget; };

    void detachDock() {
        if (dockWidget) {
            dockWidget->setWidget(nullptr);
            dockWidget = nullptr;
        }
    }

    void RootInit();
    bool IsRootItem( const GraphItem* item ) const;
    void LinkToRoot( GraphItem* item ) const;

    void AddAgent(Agent* agent, bool drawTree);
    void RemoveAgent(Agent* agent, bool drawTree);
    void RelinkAgent(const Agent* parentAgent, const Agent* childAgent, const QString &linkName, bool drawTree);
    void UnlinkAgent(const Agent* parentAgent, const Agent* childAgent, bool drawTree);

    void Clear();
    void TreeDraw() const;
    void SetLayoutDirection(GraphLayoutDirection direction);
    GraphLayoutDirection GetLayoutDirection() const { return layoutDirection; }
    void UpdateIcons() const;
    void scaleView(qreal scaleFactor);

    void SetFilterActiveOnly(bool on);
    void SetFilterWithChildOnly(bool on);
    void SetNoteField(int field, bool on);
    void SetNoteFields(const GraphNoteFields& f);
    const GraphNoteFields& GetNoteFields() const { return noteFields; }
    bool GetFilterActiveOnlyActive() const { return filterActiveOnly; }
    bool GetFilterWithChildOnly() const { return filterWithChildOnly; }
    GraphControlPanel* ControlPanel() const { return controlPanel; }
    void RefreshAllNotes() const;

    void ApplyFiltersAndLayout();

protected:
    void wheelEvent( QWheelEvent* event ) override;
    void resizeEvent( QResizeEvent* event ) override;
    void drawForeground( QPainter* painter, const QRectF& rect ) override;
};

#endif
