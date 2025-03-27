#include <UI/Graph/LayoutTreeLeft.h>

double LayoutTreeLeft::X_SEP = 450;
double LayoutTreeLeft::Y_SEP = 150;

void LayoutTreeLeft::draw( GraphItem* item )
{
    LayoutTreeLeft::NullTree( item );
    LayoutTreeLeft::ReingoldTilford( item );
    LayoutTreeLeft::DFS( item , 10, 0 );
}

void LayoutTreeLeft::NullTree( GraphItem* item )
{
    item->Modifier = 0;
    item->Thread = nullptr;
    item->Ancestor = item;

    for ( GraphItem* child : item->childItems)
        LayoutTreeLeft::NullTree( child );
}

void LayoutTreeLeft::DFS( GraphItem* item, double m, double depth )
{
    item->setPos( ( depth * X_SEP ) + 100, item->Prelim + m );

    for ( GraphItem* child : item->childItems )
        LayoutTreeLeft::DFS( child, m + item->Modifier, depth + 1 );
}

void LayoutTreeLeft::ReingoldTilford( GraphItem* item )
{
    if ( item->childItems.empty() ) {
        if ( item->parentItem && item != item->parentItem->childItems[0] ) {
            QVector<GraphItem*> children = item->parentItem->childItems;
            GraphItem* sibling = *std::prev( std::find( children.cbegin(), children.cend(), item ) );

            item->Prelim = sibling->Prelim + Y_SEP;
        } else {
            item->Prelim = 0;
        }
    } else {
        GraphItem* defaultAncestor = item->childItems[ 0 ];

        for ( GraphItem* child : item->childItems ) {
            LayoutTreeLeft::ReingoldTilford( child );
            LayoutTreeLeft::apportion( child, defaultAncestor );
        }

        LayoutTreeLeft::executeShifts( item );

        const double midpoint = ( item->childItems[ 0 ]->Prelim + item->childItems.back()->Prelim ) / 2;

        if ( item->parentItem && item != item->parentItem->childItems[ 0 ] ) {
            QVector<GraphItem*> children = item->parentItem->childItems;
            GraphItem* sibling = *std::prev( std::find( children.cbegin(), children.cend(), item ) );

            item->Prelim   = sibling->Prelim + Y_SEP;
            item->Modifier = item->Prelim - midpoint;
        } else {
            item->Prelim = midpoint;
        }
    }
}

void LayoutTreeLeft::apportion( GraphItem* item, GraphItem*& defaultAncestor )
{
    if ( item != item->parentItem->childItems[ 0 ] ) {
        QVector<GraphItem*> children = item->parentItem->childItems;
        GraphItem* sibling = *std::prev( std::find( children.cbegin(), children.cend(), item ) );

        GraphItem* vip = item;
        GraphItem* vop = item;
        GraphItem* vim = sibling;
        GraphItem* vom = vip->parentItem->childItems[ 0 ];

        double sip = vip->Modifier;
        double sop = vop->Modifier;
        double sim = vim->Modifier;
        double som = vom->Modifier;

        while ( LayoutTreeLeft::nextRight( vim ) && LayoutTreeLeft::nextLeft( vip ) ) {
            vim = LayoutTreeLeft::nextRight( vim );
            vip = LayoutTreeLeft::nextLeft( vip );
            vom = LayoutTreeLeft::nextLeft( vom );
            vop = LayoutTreeLeft::nextRight( vop );

            vop->Ancestor = item;

            double shift = ( vim->Prelim + sim ) - ( vip->Prelim + sip ) + Y_SEP;

        if ( shift > 0 ) {
            LayoutTreeLeft::moveSubtree( LayoutTreeLeft::ancestor( vim, item, defaultAncestor ), item, shift );
            sip += shift;
            sop += shift;
        }

            sim += vim->Modifier;
            sip += vip->Modifier;
            som += vom->Modifier;
            sop += vop->Modifier;
        }

        if ( LayoutTreeLeft::nextRight( vim ) && ! LayoutTreeLeft::nextRight( vop ) ) {
            vop->Thread = LayoutTreeLeft::nextRight( vim );
            vop->Modifier += sim - sop;
        }

        if ( LayoutTreeLeft::nextLeft( vip ) && ! LayoutTreeLeft::nextLeft( vom ) ) {
            vom->Thread = nextLeft( vip );
            vom->Modifier += sip - som;
            defaultAncestor = item;
        }
    }
}

GraphItem* LayoutTreeLeft::nextRight(GraphItem* item)
{
    return ( !item->childItems.empty() ) ? item->childItems.back() : item->Thread;
}

GraphItem* LayoutTreeLeft::nextLeft(GraphItem* item)
{
    return ( !item->childItems.empty() ) ? item->childItems[0] : item->Thread;
}

GraphItem* LayoutTreeLeft::ancestor(const GraphItem* vim, const GraphItem* v, GraphItem*& defaultAncestor )
{
    return (vim->Ancestor->parentItem == v->parentItem) ? vim->Ancestor : defaultAncestor;
}

void LayoutTreeLeft::executeShifts( GraphItem* item )
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

void LayoutTreeLeft::moveSubtree( GraphItem* wm, GraphItem* wp, double shift)
{
    QVector<GraphItem*> children = wm->parentItem->childItems;
    auto wmIndex  = std::distance( children.cbegin(), std::find( children.cbegin(), children.cend(), wm ) );
    auto wpIndex  = std::distance( children.cbegin(), std::find( children.cbegin(), children.cend(), wp ) );
    int  subtrees = wpIndex - wmIndex;

    if ( subtrees != 0 ) {
        wp->Change -= shift / subtrees;
        wp->Shift += shift;
        wm->Change += shift / subtrees;
        wp->Prelim += shift;
        wp->Modifier += shift;
    }
}