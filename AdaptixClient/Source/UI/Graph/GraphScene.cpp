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

            auto layoutMenu = QMenu("布局");
            auto* actionLeftToRight = layoutMenu.addAction("从左到右");
            auto* actionTopToBottom = layoutMenu.addAction("从上到下");

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


    QMenu ctxMenu;

    auto agentMenu = ctxMenu.addMenu("代理");
    agentMenu->addAction("执行命令", this, [graphics_items]() {
        bool ok = false;
        QString cmd = QInputDialog::getText(nullptr, "执行命令", "命令", QLineEdit::Normal, "", &ok);
        if (!ok)
            return;
        const auto item = dynamic_cast<GraphItem*>(graphics_items[0]);
        if (item && item->agent) {
            item->agent->Console->SetInput(cmd);
            item->agent->Console->processInput();
        }
    });
    agentMenu->addAction("任务管理器", this, [adaptixWidget, agentIds]() {
        for (const QString& agentId : agentIds) {
            adaptixWidget->TasksDock->SetAgentFilter(agentId);
            adaptixWidget->SetTasksUI();
        }
    });
    agentMenu->addSeparator();

    int agentCount = adaptixWidget->ScriptManager->AddMenuSession(agentMenu, "SessionAgent", agentIds);
    if (agentCount > 0)
        agentMenu->addSeparator();

    agentMenu->addAction("清除控制台数据", this, [adaptixWidget, agentIds]() {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "清除确认",
                                          "确定要删除服务器上所有代理控制台数据和历史记录吗（任务管理器中的任务不会被删除）？\n\n"
                                          "如需临时隐藏代理控制台内容，请通过代理控制台菜单操作。",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        for (const auto& id : agentIds)
            adaptixWidget->AgentsMap[id]->Console->Clear();

        HttpReqConsoleRemoveAsync(agentIds, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "响应超时" : message);
        });
    });
    agentMenu->addAction("从服务器删除", this, [adaptixWidget, agentIds]() {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "删除确认",
                                          "确定要从服务器删除所选代理的所有信息吗？\n\n"
                                          "如需隐藏记录，请选择：'项目 -> 在客户端隐藏'。",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        HttpReqAgentRemoveAsync(agentIds, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "响应超时" : message);
        });
    });

    auto sessionMenu = ctxMenu.addMenu("会话");
    sessionMenu->addAction("标记为活跃", this, [adaptixWidget, agentIds]() {
        HttpReqAgentSetMarkAsync(agentIds, "", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "响应超时" : message);
        });
    });
    sessionMenu->addAction("标记为非活跃", this, [adaptixWidget, agentIds]() {
        HttpReqAgentSetMarkAsync(agentIds, "Inactive", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "响应超时" : message);
        });
    });
    sessionMenu->addSeparator();
    sessionMenu->addAction("设置标签", this, [adaptixWidget, agentIds]() {
        bool inputOk;
        QString newTag = QInputDialog::getText(nullptr, "设置标签", "新标签", QLineEdit::Normal, "", &inputOk);
        if (inputOk) {
            HttpReqAgentSetTagAsync(agentIds, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
                if (!success)
                    MessageError(message.isEmpty() ? "响应超时" : message);
            });
        }
    });
    if (agentIds.size() == 1) {
        sessionMenu->addAction("设置数据", this, [adaptixWidget, agentIds]() {
            QString agentId = agentIds.first();
            if (!adaptixWidget->AgentsMap.contains(agentId))
                return;

            Agent* agent = adaptixWidget->AgentsMap[agentId];

            auto* dialog = new DialogAgentData();
            dialog->SetProfile(*(adaptixWidget->GetProfile()));
            dialog->SetAgentData(agent->data);
            dialog->Start();
        });
    }

    ctxMenu.addAction("控制台", this, [adaptixWidget, agentIds]() {
        for (const QString& agentId : agentIds) {
            adaptixWidget->LoadConsoleUI(agentId);
        }
    });
    ctxMenu.addSeparator();
    ctxMenu.addMenu(agentMenu);

    auto browserMenu = ctxMenu.addMenu("浏览器");
    int browserCount = adaptixWidget->ScriptManager->AddMenuSession(browserMenu, "SessionBrowser", agentIds);
    if (browserCount > 0)
        ctxMenu.addMenu(browserMenu);
    else
        ctxMenu.removeAction(browserMenu->menuAction());

    auto accessMenu = ctxMenu.addMenu("访问");
    int accessCount = adaptixWidget->ScriptManager->AddMenuSession(accessMenu, "SessionAccess", agentIds);
    if (accessCount > 0)
        ctxMenu.addMenu(accessMenu);
    else
        ctxMenu.removeAction(accessMenu->menuAction());

    adaptixWidget->ScriptManager->AddMenuSession(&ctxMenu, "SessionMain", agentIds);

    ctxMenu.addSeparator();
    ctxMenu.addMenu(sessionMenu);

    ctxMenu.exec(event->screenPos());
}
