#ifndef ADAPTIX_MCP_CONSOLEHANDLER_H
#define ADAPTIX_MCP_CONSOLEHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

// Forward declarations
class AdaptixWidget;

/**
 * @brief Console操作Handler
 * 
 * 提供直接操作Agent Console的能力，模拟用户输入
 * 支持命令:
 * - send_input: 向Console发送输入并执行
 * - clear_console: 清空Console输出
 * - get_output: 获取Console输出（已在InfoHandler实现）
 */
class ConsoleHandler : public IMCPCommandHandler {
public:
    explicit ConsoleHandler(AdaptixWidget* widget);
    ~ConsoleHandler() override = default;
    
    // IMCPCommandHandler interface
    QString getCommandType() const override { return "console"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Operate agent console (send input, execute commands)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    // 命令处理函数
    MCP::MCPResponse handleSendInput(const MCP::MCPRequest& request);
    MCP::MCPResponse handleClearConsole(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_CONSOLEHANDLER_H

