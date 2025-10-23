#ifndef BOFHANDLER_H
#define BOFHANDLER_H

#include "../MCPCommandHandler.h"

class AdaptixWidget;

/**
 * BOFHandler - Handler for execute_bof commands
 * 
 * This handler processes BOF (Beacon Object File) execution requests from the MCP Server.
 * It translates MCP BOF commands into AxScript commands that the client can execute.
 * 
 * Supported BOF modules:
 * - SAL-BOF (System Admin Layer)
 * - AD-BOF (Active Directory)
 * - Creds-BOF (Credential harvesting)
 * - And all other Extension-Kit BOF modules
 */
class BOFHandler : public IMCPCommandHandler {
public:
    explicit BOFHandler(AdaptixWidget* widget);
    
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;
    QString getCommandType() const override;
    QString getVersion() const override;
    bool isSupported() const override;
    QString getDescription() const override;
    
private:
    /**
     * Build AxScript command for BOF execution
     * @param module BOF module name (e.g., "SAL-BOF")
     * @param command BOF command (e.g., "dir")
     * @param args Command arguments
     * @return AxScript string
     */
    QString buildBOFScript(const QString& module, 
                          const QString& command, 
                          const QJsonObject& args);
    
    /**
     * Build argument string for a specific BOF command
     * @param command BOF command
     * @param args Arguments
     * @return Argument string
     */
    QString buildBOFArgs(const QString& command, const QJsonObject& args);
    
    AdaptixWidget* adaptixWidget;
};

#endif // BOFHANDLER_H

