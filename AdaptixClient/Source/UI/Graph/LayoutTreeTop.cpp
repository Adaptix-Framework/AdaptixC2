#include <UI/Graph/LayoutTreeTop.h>

double LayoutTreeTop::X_SEP = 230;
double LayoutTreeTop::Y_SEP = 330;

void LayoutTreeTop::draw( GraphItem* item )
{
    LayoutTreeTop::NullTree( item );
    LayoutTreeTop::ReingoldTilford( item );
    LayoutTreeTop::DFS( item , 10, 0 );
}

void LayoutTreeTop::NullTree( GraphItem* item )
{
    item->Modifier = 0;
    item->Thread = nullptr;
    item->Ancestor = item;

    for ( GraphItem* child : item->childItems)
        LayoutTreeTop::NullTree( child );
}

void LayoutTreeTop::DFS( GraphItem* item, double m, double depth )
{
    item->setPos( item->Prelim + m, ( depth * Y_SEP ) + 100 );

    for ( GraphItem* child : item->childItems )
        LayoutTreeTop::DFS( child, m + item->Modifier, depth + 1 );
}

void LayoutTreeTop::ReingoldTilford( GraphItem* item )
{
    if ( item->childItems.empty() ) {
        if ( item->parentItem && item != item->parentItem->childItems[0] ) {
            QVector<GraphItem*> children = item->parentItem->childItems;
            GraphItem* sibling = *std::prev( std::ranges::find(std::as_const(children), item ) );

            item->Prelim = sibling->Prelim + X_SEP;
        } else {
            item->Prelim = 0;
        }
    } else {
        GraphItem* defaultAncestor = item->childItems[ 0 ];

        for(int i=0; i < item->childItems.size(); i++ ) {
            GraphItem* child = item->childItems[i];
            LayoutTreeTop::ReingoldTilford( child );
            LayoutTreeTop::apportion( child, defaultAncestor );
        }

        LayoutTreeTop::executeShifts( item );

        const double midpoint = ( item->childItems[ 0 ]->Prelim + item->childItems.back()->Prelim ) / 2;

        if ( item->parentItem && item != item->parentItem->childItems[ 0 ] ) {
            QVector<GraphItem*> children = item->parentItem->childItems;
            GraphItem* sibling = *std::prev( std::ranges::find(std::as_const(children), item ) );

            item->Prelim   = sibling->Prelim + X_SEP;
            item->Modifier = item->Prelim - midpoint;
        } else {
            item->Prelim = midpoint;
        }
    }
}

void LayoutTreeTop::apportion( GraphItem* item, GraphItem*& defaultAncestor )
{
    if ( item != item->parentItem->childItems[ 0 ] ) {
        QVector<GraphItem*> children = item->parentItem->childItems;
        GraphItem* sibling = *std::prev( std::ranges::find(std::as_const(children), item ) );

        GraphItem* vip = item;
        GraphItem* vop = item;
        GraphItem* vim = sibling;
        GraphItem* vom = vip->parentItem->childItems[ 0 ];

        double sip = vip->Modifier;
        double sop = vop->Modifier;
        double sim = vim->Modifier;
        double som = vom->Modifier;

        while ( LayoutTreeTop::nextRight( vim ) && LayoutTreeTop::nextLeft( vip ) ) {
            vim = LayoutTreeTop::nextRight( vim );
            vip = LayoutTreeTop::nextLeft( vip );
            vom = LayoutTreeTop::nextLeft( vom );
            vop = LayoutTreeTop::nextRight( vop );

            vop->Ancestor = item;

            double shift = ( vim->Prelim + sim ) - ( vip->Prelim + sip ) + X_SEP;

        if ( shift > 0 ) {
            LayoutTreeTop::moveSubtree( LayoutTreeTop::ancestor( vim, item, defaultAncestor ), item, shift );
            sip += shift;
            sop += shift;
        }

            sim += vim->Modifier;
            sip += vip->Modifier;
            som += vom->Modifier;
            sop += vop->Modifier;
        }

        if ( LayoutTreeTop::nextRight( vim ) && ! LayoutTreeTop::nextRight( vop ) ) {
            vop->Thread = LayoutTreeTop::nextRight( vim );
            vop->Modifier += sim - sop;
        }

        if ( LayoutTreeTop::nextLeft( vip ) && ! LayoutTreeTop::nextLeft( vom ) ) {
            vom->Thread = nextLeft( vip );
            vom->Modifier += sip - som;
            defaultAncestor = item;
        }
    }
}

GraphItem* LayoutTreeTop::nextRight(GraphItem* item)
{
    return ( !item->childItems.empty() ) ? item->childItems.back() : item->Thread;
}

GraphItem* LayoutTreeTop::nextLeft(GraphItem* item)
{
    return ( !item->childItems.empty() ) ? item->childItems[0] : item->Thread;
}

GraphItem* LayoutTreeTop::ancestor(const GraphItem* vim, const GraphItem* v, GraphItem*& defaultAncestor )
{
    return (vim->Ancestor->parentItem == v->parentItem) ? vim->Ancestor : defaultAncestor;
}

void LayoutTreeTop::executeShifts( GraphItem* item )
{
    double shift = 0;
    double change = 0;

    for ( int i = item->childItems.size() - 1; i >= 0; i-- ) {
        GraphItem* child = item->childItems[i];
        child->Prelim += shift;
        child->Modifier += shift;
        change += child->Change;
        shift += child->Shift + change;
    }
}

void LayoutTreeTop::moveSubtree( GraphItem* wm, GraphItem* wp, double shift)
{
    QVector<GraphItem*> children = wm->parentItem->childItems;
    auto wmIndex  = std::distance( children.cbegin(), std::ranges::find(std::as_const(children), wm ) );
    auto wpIndex  = std::distance( children.cbegin(), std::ranges::find(std::as_const(children), wp ) );
    int  subtrees = wpIndex - wmIndex;

    if ( subtrees != 0 ) {
        wp->Change -= shift / subtrees;
        wp->Shift += shift;
        wm->Change += shift / subtrees;
        wp->Prelim += shift;
        wp->Modifier += shift;
    }
}
