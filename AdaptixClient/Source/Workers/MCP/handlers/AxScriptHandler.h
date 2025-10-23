#ifndef AXSCRIPTHANDLER_H
#define AXSCRIPTHANDLER_H

#include "../MCPCommandHandler.h"

class AdaptixWidget;

/**
 * AxScriptHandler - Handler for execute_axscript commands
 * 
 * This handler allows the MCP Server to execute arbitrary AxScript code,
 * providing maximum flexibility for AI-driven operations.
 * 
 * Security considerations:
 * - AxScript execution is powerful and should be used carefully
 * - Consider implementing a whitelist or sandbox
 * - Audit logging is highly recommended
 */
class AxScriptHandler : public IMCPCommandHandler {
public:
    explicit AxScriptHandler(AdaptixWidget* widget);
    
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;
    QString getCommandType() const override;
    QString getVersion() const override;
    bool isSupported() const override;
    QString getDescription() const override;
    
private:
    /**
     * Validate AxScript safety (optional security check)
     * @param script AxScript code
     * @return true if safe
     */
    bool validateScript(const QString& script);
    
    AdaptixWidget* adaptixWidget;
};

#endif // AXSCRIPTHANDLER_H

