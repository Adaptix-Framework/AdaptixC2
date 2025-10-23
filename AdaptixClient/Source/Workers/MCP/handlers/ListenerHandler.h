#ifndef ADAPTIX_MCP_LISTENERHANDLER_H
#define ADAPTIX_MCP_LISTENERHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

// Forward declarations
class AdaptixWidget;

/**
 * @brief Listener管理Handler
 * 
 * 提供Listener的查询、创建、编辑、停止功能
 * 支持命令:
 * - list: 列出所有Listener
 * - get_info: 获取特定Listener详情
 * - create: 创建新Listener
 * - edit: 编辑现有Listener
 * - stop: 停止Listener
 * - list_extenders: 列出可用的Listener类型
 */
class ListenerHandler : public IMCPCommandHandler {
public:
    explicit ListenerHandler(AdaptixWidget* widget);
    ~ListenerHandler() override = default;
    
    // IMCPCommandHandler interface
    QString getCommandType() const override { return "listener"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Manage listeners (create, edit, stop, list)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    // 命令处理函数
    MCP::MCPResponse handleListListeners(const MCP::MCPRequest& request);
    MCP::MCPResponse handleGetListenerInfo(const MCP::MCPRequest& request);
    MCP::MCPResponse handleCreateListener(const MCP::MCPRequest& request);
    MCP::MCPResponse handleEditListener(const MCP::MCPRequest& request);
    MCP::MCPResponse handleStopListener(const MCP::MCPRequest& request);
    MCP::MCPResponse handleListExtenders(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_LISTENERHANDLER_H

