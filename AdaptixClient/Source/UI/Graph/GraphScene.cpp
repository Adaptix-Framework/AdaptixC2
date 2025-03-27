#include <UI/Graph/GraphScene.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>

GraphScene::GraphScene(int gridSize, QWidget* m, QObject* parent) : QGraphicsScene(parent)
{
    this->mainWidget = m;
    this->gridSize = gridSize;
    this->setBackgroundBrush(QBrush(COLOR_Black));
}

GraphScene::~GraphScene() = default;

void GraphScene::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    QGraphicsScene::mouseMoveEvent( event );

    if ( auto item = this->mouseGrabberItem() ) {
         QPointF point = item->pos();

        double x = round( point.x() / this->gridSize ) * this->gridSize;
        double y = round( point.y() / this->gridSize ) * this->gridSize;

        item->setPos(x, y);
    }
}

void GraphScene::contextMenuEvent( QGraphicsSceneContextMenuEvent *event )
{
    auto graphics_items = selectedItems();

    if(graphics_items.empty())
        if( (graphics_items = items(event->scenePos())).empty() )
            return QGraphicsScene::contextMenuEvent( event );

    bool valid = false;
    for ( const auto& _graphics_item : graphics_items ) {
        const auto item = dynamic_cast<GraphItem*>( _graphics_item );
        if ( item && item->agent ) {
            valid = true;
            break;
        }
    }
    if (!valid)
        return;

    QMenu menu = QMenu();

    auto agentSep1 = new QAction();
    agentSep1->setSeparator(true);
    auto agentSep2 = new QAction();
    agentSep2->setSeparator(true);

    auto agentMenu = new QMenu("Agent", &menu);
    agentMenu->addAction("Tasks");
    agentMenu->addAction(agentSep1);
    agentMenu->addAction("File Browser");
    agentMenu->addAction("Process Browser");
    agentMenu->addAction(agentSep2);
    agentMenu->addAction("Exit");

    auto itemMenu = new QMenu("Item", &menu);
    itemMenu->addAction("Mark as Active");
    itemMenu->addAction("Mark as Inactive");

    menu.addAction("Console");
    menu.addSeparator();
    menu.addMenu(agentMenu);
    menu.addMenu(itemMenu);
    menu.addSeparator();
    menu.addAction("Remove from server");

    const auto action = menu.exec( event->screenPos() );
    if ( !action )
        return;

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    if ( action->text() == "Console" ) {
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                adaptixWidget->LoadConsoleUI(item->agent->data.Id);
        }
    }
    else if ( action->text() == "Tasks") {
        const auto item = dynamic_cast<GraphItem*>( graphics_items[0] );
        if ( item && item->agent) {
            adaptixWidget->TasksTab->SetAgentFilter(item->agent->data.Id);
            adaptixWidget->SetTasksUI();
        }
    }
    else if ( action->text() == "File Browser" ) {
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                adaptixWidget->LoadFileBrowserUI(item->agent->data.Id);
        }
    }
    else if ( action->text() == "Process Browser" ) {
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                adaptixWidget->LoadProcessBrowserUI(item->agent->data.Id);
        }
    }
    else if ( action->text() ==  "Exit" ) {
        QStringList listId;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                listId.append(item->agent->data.Id);
        }
        if(listId.empty())
            return;

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentExit(listId, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
    else if ( action->text() == "Mark as Active" ) {
        QStringList listId;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                listId.append(item->agent->data.Id);
        }
        if(listId.empty())
            return;

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentSetMark(listId, "", *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
    else if ( action->text() == "Mark as Inactive"  ) {
        QStringList listId;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                listId.append(item->agent->data.Id);
        }
        if(listId.empty())
            return;

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentSetMark(listId, "Inactive", *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
    else if ( action->text() == "Remove from server" ) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Delete Confirmation",
                                          "Are you sure you want to delete all information about the selected agents from the server?\n\n"
                                          "If you want to hide the record, simply choose: 'Item -> Hide on Client'.",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        QStringList listId;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent)
                listId.append(item->agent->data.Id);
        }
        if(listId.empty())
            return;

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentRemove(listId, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
}
