#include <Agent/Agent.h>
#include <UI/Graph/GraphScene.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <UI/Dialogs/DialogAgentData.h>
#include <UI/Graph/SessionsGraph.h>


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
        if( (graphics_items = items(event->scenePos())).empty() ) {
            auto* sessionsGraph = qobject_cast<SessionsGraph*>(parent());
            if (!sessionsGraph)
                return QGraphicsScene::contextMenuEvent( event );

            auto layoutMenu = QMenu("Layout");
            auto* actionLeftToRight = layoutMenu.addAction("Left to Right");
            auto* actionTopToBottom = layoutMenu.addAction("Top to Bottom");

            actionLeftToRight->setCheckable(true);
            actionTopToBottom->setCheckable(true);
            actionLeftToRight->setChecked(sessionsGraph->GetLayoutDirection() == LayoutLeftToRight);
            actionTopToBottom->setChecked(sessionsGraph->GetLayoutDirection() == LayoutTopToBottom);

            auto ctxMenu = QMenu();
            ctxMenu.addMenu(&layoutMenu);

            const auto action = ctxMenu.exec(event->screenPos());
            if (action == actionLeftToRight) {
                sessionsGraph->SetLayoutDirection(LayoutLeftToRight);
            } else if (action == actionTopToBottom) {
                sessionsGraph->SetLayoutDirection(LayoutTopToBottom);
            }
            return;
        }
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
    sessionMenu.addSeparator();
    sessionMenu.addAction("Set tag");
    if (agentIds.size() == 1 )
        sessionMenu.addAction("Set data");

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

        HttpReqConsoleRemoveAsync(agentIds, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
    else if ( action->text() == "Remove from server" ) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Delete Confirmation",
                                          "Are you sure you want to delete all information about the selected agents from the server?\n\n"
                                          "If you want to hide the record, simply choose: 'Item -> Hide on Client'.",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        HttpReqAgentRemoveAsync(agentIds, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
    else if ( action->text() == "Mark as Active" ) {
        HttpReqAgentSetMarkAsync(agentIds, "", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
    else if ( action->text() == "Mark as Inactive" ) {
        HttpReqAgentSetMarkAsync(agentIds, "Inactive", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
    else if ( action->text() == "Set tag" ) {
        QString tag = "";
        bool inputOk;
        QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
        if ( inputOk ) {
            HttpReqAgentSetTagAsync(agentIds, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
                if (!success)
                    MessageError(message.isEmpty() ? "Response timeout" : message);
            });
        }
    }
    else if ( action->text() == "Set data" ) {
        if (agentIds.isEmpty())
            return;

        QString agentId = agentIds.first();
        if (!adaptixWidget->AgentsMap.contains(agentId))
            return;

        Agent* agent = adaptixWidget->AgentsMap[agentId];

        auto* dialog = new DialogAgentData();
        dialog->SetProfile(*(adaptixWidget->GetProfile()));
        dialog->SetAgentData(agent->data);
        dialog->Start();
    }
}
