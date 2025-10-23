#include "ConsoleHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>
#include <QMetaObject>
#include <QThread>

using namespace MCP;

ConsoleHandler::ConsoleHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse ConsoleHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "send_input") {
        return handleSendInput(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown console command: %1. Use: send_input").arg(command)
        );
    }
}

MCPResponse ConsoleHandler::handleSendInput(const MCPRequest& request)
{
    QString agentId = request.params["agent_id"].toString();
    QString input = request.params["input"].toString();
    
    if (agentId.isEmpty()) {
        return MCPResponse::error(request.requestId, 
            "Missing required parameter: agent_id");
    }
    
    if (input.isEmpty()) {
        return MCPResponse::error(request.requestId, 
            "Missing required parameter: input");
    }
    
    // 获取Agent
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent) {
        return MCPResponse::error(
            request.requestId,
            QString("Agent '%1' not found").arg(agentId)
        );
    }
    
    // 获取Agent的Console
    if (!agent->Console) {
        return MCPResponse::error(
            request.requestId,
            QString("Agent '%1' has no console").arg(agentId)
        );
    }
    
    // 在主线程中执行Console操作
    // 使用SetInput设置输入，然后调用processInput处理
    bool success = false;
    
    QMetaObject::invokeMethod(agent->Console, [agent, input, &success]() {
        if (agent->Console) {
            // 设置输入框文本
            agent->Console->SetInput(input);
            
            // 调用processInput槽函数，模拟用户按下回车
            agent->Console->processInput();
            
            success = true;
        }
    }, Qt::BlockingQueuedConnection);
    
    if (success) {
        QJsonObject data;
        data["agent_id"] = agentId;
        data["input"] = input;
        
        return MCPResponse::success(
            request.requestId,
            QString("Command sent to agent %1: %2").arg(agentId).arg(input),
            data
        );
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Failed to send command to agent %1").arg(agentId)
        );
    }
}

