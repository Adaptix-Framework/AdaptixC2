#ifndef MCPCOMMANDHANDLER_H
#define MCPCOMMANDHANDLER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include "MCPProtocol.h"

class AdaptixWidget;

/**
 * IMCPCommandHandler - Interface for MCP command handlers
 * 
 * Each command type has its own handler implementing this interface.
 * Handlers are registered in a central registry for extensibility.
 */
class IMCPCommandHandler {
public:
    virtual ~IMCPCommandHandler() = default;
    
    /**
     * Handle the MCP command
     * @param request The MCP request
     * @return MCP response
     */
    virtual MCP::MCPResponse handle(const MCP::MCPRequest& request) = 0;
    
    /**
     * Get the command type this handler supports
     * @return Command type string
     */
    virtual QString getCommandType() const = 0;
    
    /**
     * Get the handler version
     * @return Version string (e.g., "1.0")
     */
    virtual QString getVersion() const { return "1.0"; }
    
    /**
     * Check if this handler is currently supported/available
     * @return true if supported
     */
    virtual bool isSupported() const { return true; }
    
    /**
     * Get handler description
     * @return Description string
     */
    virtual QString getDescription() const { return ""; }
    
    /**
     * Get capability information
     * @return Capability struct
     */
    virtual MCP::Capability getCapability() const {
        MCP::Capability cap;
        cap.name = getCommandType();
        cap.version = getVersion();
        cap.description = getDescription();
        cap.available = isSupported();
        return cap;
    }
};

/**
 * MCPCommandRegistry - Central registry for MCP command handlers
 * 
 * Singleton pattern for managing all command handlers.
 * Supports dynamic registration and lookup.
 */
class MCPCommandRegistry {
public:
    /**
     * Get the singleton instance
     */
    static MCPCommandRegistry* instance();
    
    /**
     * Register a command handler
     * @param type Command type
     * @param handler Handler instance (ownership transferred to registry)
     */
    void registerHandler(const QString& type, IMCPCommandHandler* handler);
    
    /**
     * Get a handler for a command type
     * @param type Command type
     * @return Handler instance or nullptr if not found
     */
    IMCPCommandHandler* getHandler(const QString& type);
    
    /**
     * Check if a command type is supported
     * @param type Command type
     * @return true if supported
     */
    bool isSupported(const QString& type);
    
    /**
     * Get list of all supported command types
     * @return List of command type strings
     */
    QStringList getSupportedCommands() const;
    
    /**
     * Get all capabilities
     * @return List of capabilities
     */
    QList<MCP::Capability> getAllCapabilities() const;
    
    /**
     * Clear all handlers (for cleanup)
     */
    void clear();
    
private:
    MCPCommandRegistry() = default;
    ~MCPCommandRegistry();
    
    // Prevent copying
    MCPCommandRegistry(const MCPCommandRegistry&) = delete;
    MCPCommandRegistry& operator=(const MCPCommandRegistry&) = delete;
    
    QMap<QString, IMCPCommandHandler*> handlers;
    mutable QMutex mutex;
    
    static MCPCommandRegistry* s_instance;
};

/**
 * Auto-registration helper macro
 * 
 * Usage in handler implementation:
 *   REGISTER_MCP_HANDLER(MyHandler, widget)
 */
#define REGISTER_MCP_HANDLER(HandlerClass, widgetPtr) \
    static HandlerClass* g_##HandlerClass##_instance = nullptr; \
    if (!g_##HandlerClass##_instance) { \
        g_##HandlerClass##_instance = new HandlerClass(widgetPtr); \
        MCPCommandRegistry::instance()->registerHandler( \
            g_##HandlerClass##_instance->getCommandType(), \
            g_##HandlerClass##_instance \
        ); \
    }

#endif // MCPCOMMANDHANDLER_H

