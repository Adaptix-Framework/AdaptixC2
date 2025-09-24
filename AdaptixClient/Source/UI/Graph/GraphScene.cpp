#include <Agent/Agent.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>


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
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if (!adaptixWidget)
        return;

    auto graphics_items = selectedItems();
    if(graphics_items.empty()) {
        if( (graphics_items = items(event->scenePos())).empty() )
            return QGraphicsScene::contextMenuEvent( event );
    }

    QStringList agentIds;
    for ( const auto& _graphics_item : graphics_items ) {
        const auto item = dynamic_cast<GraphItem*>( _graphics_item );
        if ( item && item->agent )
            agentIds.append(item->agent->data.Id);
    }
    if (agentIds.size() == 0)
        return;





    auto agentMenu = QMenu("Agent");
    agentMenu.addAction("Execute command");
    agentMenu.addAction("Task manager");
    agentMenu.addSeparator();

    int agentCount = adaptixWidget->ScriptManager->AddMenuSession(&agentMenu, "SessionAgent", agentIds);
    if (agentCount > 0)
        agentMenu.addSeparator();

    agentMenu.addAction("Remove console data");
    agentMenu.addAction("Remove from server");



    auto sessionMenu = QMenu("Session");
    sessionMenu.addAction("Mark as Active");
    sessionMenu.addAction("Mark as Inactive");



    auto ctxMenu = QMenu();
    ctxMenu.addAction("Console");
    ctxMenu.addSeparator();
    ctxMenu.addMenu(&agentMenu);

    auto browserMenu = QMenu("Browsers");
    int browserCount = adaptixWidget->ScriptManager->AddMenuSession(&browserMenu, "SessionBrowser", agentIds);
    if (browserCount > 0)
        ctxMenu.addMenu(&browserMenu);

    auto accessMenu = QMenu("Access");
    int accessCount = adaptixWidget->ScriptManager->AddMenuSession(&accessMenu, "SessionAccess", agentIds);
    if (accessCount > 0)
        ctxMenu.addMenu(&accessMenu);

    adaptixWidget->ScriptManager->AddMenuSession(&ctxMenu, "SessionMain", agentIds);

    ctxMenu.addSeparator();
    ctxMenu.addMenu(&sessionMenu);
    ctxMenu.addAction("Set tag");

    const auto action = ctxMenu.exec( event->screenPos() );
    if ( !action )
        return;

    if ( action->text() == "Console" ) {
        for (QString agentId : agentIds) {
            adaptixWidget->LoadConsoleUI(agentId);
        }
    }
    else if ( action->text() == "Execute command") {
        bool ok = false;
        QString cmd = QInputDialog::getText(nullptr,"Execute Command", "Command", QLineEdit::Normal, "", &ok);
        if (!ok)
            return;

        const auto item = dynamic_cast<GraphItem*>( graphics_items[0] );
        if ( item && item->agent) {
            item->agent->Console->SetInput(cmd);
            item->agent->Console->processInput();
        }
    }
    else if ( action->text() == "Task manager") {
        for (QString agentId : agentIds) {
            adaptixWidget->TasksDock->SetAgentFilter(agentId);
            adaptixWidget->SetTasksUI();
        }
    }
    else if ( action->text() == "Remove console data" ) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Clear Confirmation",
                                          "Are you sure you want to delete all agent console data and history from server (tasks will not be deleted from TaskManager)?\n\n"
                                          "If you want to temporarily hide the contents of the agent console, do so through the agent console menu.",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        for (auto id : agentIds)
            adaptixWidget->AgentsMap[id]->Console->Clear();

        QString message = QString();
        bool ok = false;
        bool result = HttpReqConsoleRemove(agentIds, *(adaptixWidget->GetProfile()), &message, &ok);
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

        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentRemove(agentIds, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
    else if ( action->text() == "Mark as Active" ) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentSetMark(agentIds, "", *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
    else if ( action->text() == "Mark as Inactive" ) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentSetMark(agentIds, "Inactive", *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
    else if ( action->text() == "Set tag" ) {
        QString tag = "";
        bool inputOk;
        QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
        if ( inputOk ) {
            QString message = QString();
            bool ok = false;
            bool result = HttpReqAgentSetTag(agentIds, newTag, *(adaptixWidget->GetProfile()), &message, &ok);
            if( !result ) {
                MessageError("Response timeout");
                return;
            }
        }
    }
}
