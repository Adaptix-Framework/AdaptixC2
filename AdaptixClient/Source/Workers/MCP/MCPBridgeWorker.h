#ifndef MCPBRIDGEWORKER_H
#define MCPBRIDGEWORKER_H

#include <QObject>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QMutex>
#include "MCPProtocol.h"
#include "MCPCommandHandler.h"

class AdaptixWidget;

/**
 * MCPBridgeWorker - WebSocket server for MCP communication
 * 
 * This class implements a local WebSocket server that allows the MCP Server (Go)
 * to communicate with the AdaptixClient, enabling AI-driven operations.
 * 
 * Features:
 * - Listens on localhost:9999 (configurable)
 * - Processes MCP commands (BOF execution, AxScript, etc.)
 * - Extensible command handler system
 * - Thread-safe operation
 * 
 * Security:
 * - Only binds to localhost
 * - Optional authentication token
 * - Command whitelist support
 * - Audit logging
 */
class MCPBridgeWorker : public QObject {
    Q_OBJECT
    
public:
    /**
     * Constructor
     * @param widget Pointer to AdaptixWidget for accessing client features
     * @param port Listen port (default: 9999)
     * @param parent Parent QObject
     */
    explicit MCPBridgeWorker(AdaptixWidget* widget, 
                            int port = 9999, 
                            QObject* parent = nullptr);
    
    /**
     * Destructor
     */
    ~MCPBridgeWorker();
    
    /**
     * Check if the bridge is running
     * @return true if running
     */
    bool isRunning() const { return wsServer != nullptr && wsServer->isListening(); }
    
    /**
     * Get the listen port
     * @return Port number
     */
    quint16 getPort() const;
    
    /**
     * Get the number of active connections
     * @return Connection count
     */
    int getConnectionCount() const;
    
    /**
     * Set authentication token (optional)
     * @param token Authentication token
     */
    void setAuthToken(const QString& token);
    
    /**
     * Enable/disable audit logging
     * @param enabled true to enable
     */
    void setAuditLogEnabled(bool enabled);
    
Q_SIGNALS:
    /**
     * Emitted when the bridge starts successfully
     * @param port The listen port
     */
    void started(quint16 port);
    
    /**
     * Emitted when the bridge stops
     */
    void stopped();
    
    /**
     * Emitted when a new MCP connection is established
     * @param peerAddress Peer address
     */
    void connectionEstablished(QString peerAddress);
    
    /**
     * Emitted when an MCP connection is closed
     */
    void connectionClosed();
    
    /**
     * Emitted when a command is executed
     * @param type Command type
     * @param success Whether execution succeeded
     */
    void commandExecuted(QString type, bool success);
    
    /**
     * Emitted on error
     * @param error Error message
     */
    void errorOccurred(QString error);
    
public Q_SLOTS:
    /**
     * Start the MCP bridge
     * @return true if started successfully
     */
    bool start();
    
    /**
     * Stop the MCP bridge
     */
    void stop();
    
private Q_SLOTS:
    /**
     * Handle new WebSocket connection
     */
    void onNewConnection();
    
    /**
     * Handle incoming text message
     * @param message JSON message
     */
    void onTextMessageReceived(const QString& message);
    
    /**
     * Handle WebSocket disconnection
     */
    void onDisconnected();
    
    /**
     * Handle WebSocket error
     * @param error Error code
     */
    void onSocketError(QAbstractSocket::SocketError error);
    
private:
    /**
     * Process an MCP request
     * @param request MCP request
     * @return MCP response
     */
    MCP::MCPResponse processRequest(const MCP::MCPRequest& request);
    
    /**
     * Handle built-in commands (ping, get_capabilities, etc.)
     * @param request MCP request
     * @return MCP response
     */
    MCP::MCPResponse handleBuiltinCommand(const MCP::MCPRequest& request);
    
    /**
     * Validate request authentication
     * @param request MCP request
     * @return true if valid
     */
    bool validateAuth(const MCP::MCPRequest& request);
    
    /**
     * Log command execution (audit)
     * @param request MCP request
     * @param response MCP response
     */
    void logCommand(const MCP::MCPRequest& request, 
                   const MCP::MCPResponse& response);
    
    /**
     * Send response to client
     * @param response MCP response
     */
    void sendResponse(const MCP::MCPResponse& response);
    
    // WebSocket server
    QWebSocketServer* wsServer;
    
    // Current MCP connection (only one allowed)
    QWebSocket* mcpConnection;
    
    // AdaptixWidget reference for accessing client features
    AdaptixWidget* adaptixWidget;
    
    // Configuration
    int listenPort;
    QString authToken;
    bool auditLogEnabled;
    
    // Thread safety
    mutable QMutex connectionMutex;
};

#endif // MCPBRIDGEWORKER_H

