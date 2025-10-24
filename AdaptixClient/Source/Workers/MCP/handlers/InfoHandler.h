#ifndef INFOHANDLER_H
#define INFOHANDLER_H

#include "../MCPCommandHandler.h"

class AdaptixWidget;

/**
 * InfoHandler - Query client information (agents, listeners, tasks, etc.)
 * 
 * Supported commands:
 * - list_agents: Get all connected agents
 * - list_listeners: Get all active listeners
 * - get_agent_info: Get detailed info for a specific agent
 * - get_agent_console: Get agent console output
 */
class InfoHandler : public IMCPCommandHandler {
public:
    explicit InfoHandler(AdaptixWidget* widget);
    ~InfoHandler() override = default;
    
    // IMCPCommandHandler interface
    QString getCommandType() const override { return "info"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override;
    QString getDescription() const override;
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    // Command implementations
    MCP::MCPResponse handleListAgents(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListListeners(const MCP::MCPRequest& request);
    MCP::MCPResponse handleGetAgentInfo(const MCP::MCPRequest& request);
    MCP::MCPResponse handleGetAgentConsole(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListCredentials(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListDownloads(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListScreenshots(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListTasks(const MCP::MCPRequest& request);
    MCP::MCPResponse handleGetTaskOutput(const MCP::MCPRequest& request);
};

#endif // INFOHANDLER_H

