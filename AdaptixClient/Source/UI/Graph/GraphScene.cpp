#include <Agent/Agent.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Dialogs/DialogTunnel.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>

GraphScene::GraphScene(const int gridSize, QWidget* m, QObject* parent) : QGraphicsScene(parent)
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

    bool menuRemoteTerminal = false;
    bool menuProcessBrowser = false;
    bool menuFileBrowser = false;
    bool menuExit = false;
    bool menuTunnels = false;

    bool tunnelS5 = false;
    bool tunnelS4 = false;
    bool tunnelLpf = false;
    bool tunnelRpf = false;

    int selectedCount = 0;

    bool valid = false;
    for ( const auto& _graphics_item : graphics_items ) {
        const auto item = dynamic_cast<GraphItem*>( _graphics_item );
        if ( item && item->agent ) {
            valid = true;

            menuRemoteTerminal = item->agent->browsers.RemoteTerminal;
            menuFileBrowser    = item->agent->browsers.FileBrowser;
            menuProcessBrowser = item->agent->browsers.ProcessBrowser;
            menuTunnels        = item->agent->browsers.SessionsMenuTunnels;
            menuExit           = item->agent->browsers.SessionsMenuExit;

            selectedCount++;
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
    if (menuFileBrowser || menuProcessBrowser || menuTunnels || menuRemoteTerminal) {
        agentMenu->addAction(agentSep1);
        if (menuRemoteTerminal)
            agentMenu->addAction("Remote Terminal");
        if (menuFileBrowser)
            agentMenu->addAction("File Browser");
        if (menuProcessBrowser)
            agentMenu->addAction("Process Browser");
        if (menuTunnels && selectedCount == 1)
            agentMenu->addAction("Create Tunnel");
    }
    if (menuExit) {
        agentMenu->addAction(agentSep2);
        agentMenu->addAction("Exit");
    }

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
    if (!adaptixWidget)
        return;

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
    else if ( action->text() == "Remote Terminal" ) {
        for ( const auto& _graphics_item : graphics_items ) {
           const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent )
                adaptixWidget->LoadTerminalUI(item->agent->data.Id);
        }
    }
    else if ( action->text() == "File Browser" ) {
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent )
                adaptixWidget->LoadFileBrowserUI(item->agent->data.Id);
        }
    }
    else if ( action->text() == "Process Browser" ) {
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent )
                adaptixWidget->LoadProcessBrowserUI(item->agent->data.Id);
        }
    }
    else if ( action->text() == "Create Tunnel" ) {
        Agent* agent = nullptr;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent ) {
                agent = item->agent;
                break;
            }
        }

        if (!agent)
            return;


        DialogTunnel dialogTunnel;
        dialogTunnel.SetSettings(agent->data.Id, agent->browsers.Socks5, agent->browsers.Socks4, agent->browsers.Lportfwd, agent->browsers.Rportfwd);

        while (true) {
            dialogTunnel.StartDialog();
            if (dialogTunnel.IsValid())
                break;

            QString msg = dialogTunnel.GetMessage();
            if (msg.isEmpty())
                return;

            MessageError(msg);
        }

        QString    tunnelType = dialogTunnel.GetTunnelType();
        QByteArray tunnelData = dialogTunnel.GetTunnelData();

        QString message = QString();
        bool ok = false;
        bool result = HttpReqTunnelStartServer(tunnelType, tunnelData, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Server is not responding");
            return;
        }
        if ( !ok ) {
            MessageError(message);
            return;
        }

        agent = nullptr;
    }
    else if ( action->text() ==  "Exit" ) {
        QStringList listId;
        for ( const auto& _graphics_item : graphics_items ) {
            const auto item = dynamic_cast<GraphItem*>( _graphics_item );
            if ( item && item->agent && item->agent->browsers.SessionsMenuExit )
                listId.append(item->agent->data.Id);
        }
        if(listId.empty())
            return;

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentExit(listId, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
        MessageError("Response timeout");
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
        MessageError("Response timeout");
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
            MessageError("Response timeout");
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
            MessageError("Response timeout");
            return;
        }
    }
}
