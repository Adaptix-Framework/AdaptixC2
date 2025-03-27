#ifndef ADAPTIXCLIENT_LAYOUTTREELEFT_H
#define ADAPTIXCLIENT_LAYOUTTREELEFT_H

#include <UI/Graph/GraphItem.h>

class LayoutTreeLeft
{
public:
     static double X_SEP;
     static double Y_SEP;

     static void draw( GraphItem* item );

     static void NullTree( GraphItem* item );

     static void ReingoldTilford( GraphItem* item );

     static void DFS( GraphItem* item, double m, double depth );

     static void apportion(GraphItem* item, GraphItem*& defaultAncestor);

     static GraphItem* nextRight(GraphItem* v);
     static GraphItem* nextLeft(GraphItem* v);

     static void moveSubtree( GraphItem* wm, GraphItem* wp, double shift);

     static GraphItem* ancestor(const GraphItem* vim, const GraphItem* v, GraphItem*& defaultAncestor );

     static void executeShifts( GraphItem* item );
};

#endif //ADAPTIXCLIENT_LAYOUTTREELEFT_H
