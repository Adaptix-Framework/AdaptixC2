#include "AxScriptHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>
#include <Agent/Commander.h>
#include <Client/AxScript/AxScriptManager.h>
#include <QJSEngine>

AxScriptHandler::AxScriptHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

QString AxScriptHandler::getCommandType() const {
    return MCP::Commands::EXECUTE_AXSCRIPT;
}

QString AxScriptHandler::getVersion() const {
    return "1.0";
}

bool AxScriptHandler::isSupported() const {
    return adaptixWidget != nullptr && adaptixWidget->AxConsoleDock != nullptr;
}

QString AxScriptHandler::getDescription() const {
    return "Execute arbitrary AxScript code on agents";
}

MCP::MCPResponse AxScriptHandler::handle(const MCP::MCPRequest& request) {
    // Extract required parameters
    QString agentId = request.params["agent_id"].toString();
    QString script = request.params["script"].toString();
    
    // Validate parameters
    if (agentId.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: agent_id");
    }
    
    if (script.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: script");
    }
    
    // Optional security validation
    if (!validateScript(script)) {
        qWarning() << "[AxScript Handler] Script validation failed";
        return MCP::MCPResponse::error(request.requestId, 
            "Script validation failed (potential security risk)");
    }
    
    qDebug() << "[AxScript Handler] Executing script on agent:" << agentId;
    qDebug() << "[AxScript Handler] Script:" << script.left(100) 
             << (script.length() > 100 ? "..." : "");
    
    // Get agent's script engine
    QJSEngine* agentEngine = adaptixWidget->ScriptManager->AgentScriptEngine(agentId);
    if (!agentEngine) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Agent '%1' not found or has no script engine").arg(agentId)
        );
    }
    
    // Execute script via agent's engine
    QJSValue result = agentEngine->evaluate(script);
    bool success = !result.isError();
    
    if (success) {
        QJsonObject data;
        data["agent_id"] = agentId;
        data["script_length"] = script.length();
        data["result"] = result.toString();
        
        return MCP::MCPResponse::success(
            request.requestId,
            QString("AxScript executed on agent %1").arg(agentId),
            data
        );
    } else {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("AxScript error: %1").arg(result.toString())
        );
    }
}

bool AxScriptHandler::validateScript(const QString& script) {
    // Basic safety checks (can be expanded)
    
    // Check script length (prevent extremely long scripts)
    if (script.length() > 100000) {
        qWarning() << "[AxScript Handler] Script too long:" << script.length() << "chars";
        return false;
    }
    
    // Whitelist check: if script contains certain dangerous patterns, reject
    // (This is a basic example; a real implementation might use a proper parser)
    QStringList dangerousPatterns = {
        // Add patterns if needed for your security policy
        // For now, we allow all scripts since this is internal communication
    };
    
    for (const QString& pattern : dangerousPatterns) {
        if (script.contains(pattern, Qt::CaseInsensitive)) {
            qWarning() << "[AxScript Handler] Dangerous pattern detected:" << pattern;
            return false;
        }
    }
    
    return true;
}

