#include "BOFHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>
#include <Agent/Commander.h>

BOFHandler::BOFHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

QString BOFHandler::getCommandType() const {
    return MCP::Commands::EXECUTE_BOF;
}

QString BOFHandler::getVersion() const {
    return "1.0";
}

bool BOFHandler::isSupported() const {
    // BOF execution is always supported if client has AxScript engine
    return adaptixWidget != nullptr && adaptixWidget->AxConsoleDock != nullptr;
}

QString BOFHandler::getDescription() const {
    return "Execute BOF (Beacon Object File) commands on agents";
}

MCP::MCPResponse BOFHandler::handle(const MCP::MCPRequest& request) {
    // Extract required parameters
    QString agentId = request.params["agent_id"].toString();
    QString bofModule = request.params["bof_module"].toString();
    QString command = request.params["command"].toString();
    QJsonObject args = request.params["args"].toObject();
    
    // Validate parameters
    if (agentId.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: agent_id");
    }
    
    if (command.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: command");
    }
    
    qDebug() << "[BOF Handler] Executing BOF:" << command 
             << "on agent:" << agentId;
    
    // Get the agent's commander to execute the command
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    
    if (!agent || !agent->commander) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Agent '%1' not found or has no commander").arg(agentId)
        );
    }
    
    // Execute command via Commander (same way ConsoleWidget does it)
    CommanderResult cmdResult = agent->commander->ProcessInput(agentId, command);
    
    bool success = !cmdResult.error;
    
    if (success) {
        QJsonObject data;
        data["agent_id"] = agentId;
        data["bof_module"] = bofModule;
        data["command"] = command;
        data["message"] = cmdResult.message;
        
        return MCP::MCPResponse::success(
            request.requestId,
            QString("BOF task '%1' created for agent %2").arg(command).arg(agentId),
            data
        );
    } else {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Failed to execute BOF '%1' on agent %2").arg(command).arg(agentId)
        );
    }
}
