#include "MCPBridgeWorker.h"
#include "UI/Widgets/AdaptixWidget.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

MCPBridgeWorker::MCPBridgeWorker(AdaptixWidget* widget, int port, QObject* parent)
    : QObject(parent)
    , wsServer(nullptr)
    , mcpConnection(nullptr)
    , adaptixWidget(widget)
    , listenPort(port)
    , authToken("")
    , auditLogEnabled(true)
{
    Q_ASSERT(widget != nullptr);
}

MCPBridgeWorker::~MCPBridgeWorker() {
    stop();
}

bool MCPBridgeWorker::start() {
    if (wsServer && wsServer->isListening()) {
        qWarning() << "[MCP Bridge] Already running on port" << wsServer->serverPort();
        return true;
    }
    
    // Create WebSocket server (non-secure, localhost only)
    wsServer = new QWebSocketServer(
        "AdaptixC2 MCP Bridge",
        QWebSocketServer::NonSecureMode,
        this
    );
    
    // Connect signals
    connect(wsServer, &QWebSocketServer::newConnection,
            this, &MCPBridgeWorker::onNewConnection);
    
    // Listen on localhost only (security)
    if (wsServer->listen(QHostAddress::LocalHost, listenPort)) {
        quint16 actualPort = wsServer->serverPort();
        qInfo() << "[MCP Bridge] 🚀 Started on ws://localhost:" << actualPort;
        Q_EMIT started(actualPort);
        return true;
    } else {
        QString error = wsServer->errorString();
        qCritical() << "[MCP Bridge] ❌ Failed to start:" << error;
        Q_EMIT errorOccurred(error);
        delete wsServer;
        wsServer = nullptr;
        return false;
    }
}

void MCPBridgeWorker::stop() {
    if (mcpConnection) {
        mcpConnection->close();
        mcpConnection = nullptr;
    }
    
    if (wsServer) {
        wsServer->close();
        delete wsServer;
        wsServer = nullptr;
        qInfo() << "[MCP Bridge] Stopped";
        Q_EMIT stopped();
    }
}

quint16 MCPBridgeWorker::getPort() const {
    return wsServer ? wsServer->serverPort() : 0;
}

int MCPBridgeWorker::getConnectionCount() const {
    QMutexLocker locker(&connectionMutex);
    return mcpConnection ? 1 : 0;
}

void MCPBridgeWorker::setAuthToken(const QString& token) {
    authToken = token;
    qDebug() << "[MCP Bridge] Authentication token" << (token.isEmpty() ? "disabled" : "enabled");
}

void MCPBridgeWorker::setAuditLogEnabled(bool enabled) {
    auditLogEnabled = enabled;
    qDebug() << "[MCP Bridge] Audit logging" << (enabled ? "enabled" : "disabled");
}

void MCPBridgeWorker::onNewConnection() {
    QMutexLocker locker(&connectionMutex);
    
    // Only allow one MCP connection
    if (mcpConnection) {
        QWebSocket* newConn = wsServer->nextPendingConnection();
        qWarning() << "[MCP Bridge] Rejecting connection (already have active connection)";
        newConn->close();
        newConn->deleteLater();
        return;
    }
    
    mcpConnection = wsServer->nextPendingConnection();
    
    // Connect signals
    connect(mcpConnection, &QWebSocket::textMessageReceived,
            this, &MCPBridgeWorker::onTextMessageReceived);
    connect(mcpConnection, &QWebSocket::disconnected,
            this, &MCPBridgeWorker::onDisconnected);
    connect(mcpConnection, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &MCPBridgeWorker::onSocketError);
    
    QString peerAddress = mcpConnection->peerAddress().toString();
    qInfo() << "[MCP Bridge] ✅ MCP Server connected from" << peerAddress;
    Q_EMIT connectionEstablished(peerAddress);
}

void MCPBridgeWorker::onTextMessageReceived(const QString& message) {
    // Parse JSON request
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[MCP Bridge] Invalid JSON received";
        MCP::MCPResponse errorResp = MCP::MCPResponse::error("", "Invalid JSON");
        sendResponse(errorResp);
        return;
    }
    
    // Parse MCP request
    MCP::MCPRequest request = MCP::MCPRequest::fromJson(doc.object());
    if (!request.isValid()) {
        qWarning() << "[MCP Bridge] Invalid MCP request";
        MCP::MCPResponse errorResp = MCP::MCPResponse::error(
            request.requestId, "Invalid request: missing type or request_id"
        );
        sendResponse(errorResp);
        return;
    }
    
    qDebug() << "[MCP Bridge] 📥 Received command:" << request.type 
             << "| ID:" << request.requestId;
    
    // Validate authentication
    if (!validateAuth(request)) {
        qWarning() << "[MCP Bridge] Authentication failed for request" << request.requestId;
        MCP::MCPResponse errorResp = MCP::MCPResponse::error(
            request.requestId, "Authentication failed"
        );
        sendResponse(errorResp);
        return;
    }
    
    // Process request
    MCP::MCPResponse response = processRequest(request);
    
    // Log command (audit)
    if (auditLogEnabled) {
        logCommand(request, response);
    }
    
    // Send response
    sendResponse(response);
    
    // Emit signal
    bool success = (response.status == MCP::Status::SUCCESS);
    Q_EMIT commandExecuted(request.type, success);
}

void MCPBridgeWorker::onDisconnected() {
    QMutexLocker locker(&connectionMutex);
    
    qInfo() << "[MCP Bridge] 📡 MCP Server disconnected (normal close)";
    
    if (mcpConnection) {
        mcpConnection->deleteLater();
        mcpConnection = nullptr;
    }
    
    Q_EMIT connectionClosed();
}

void MCPBridgeWorker::onSocketError(QAbstractSocket::SocketError error) {
    // Distinguish between normal disconnect and actual errors
    QString errorMsg;
    
    switch (error) {
        case QAbstractSocket::RemoteHostClosedError:
            // Normal disconnect - client closed connection gracefully
            qDebug() << "[MCP Bridge] 👋 Client closed connection";
            return; // Don't emit error for normal close
            
        case QAbstractSocket::SocketTimeoutError:
            errorMsg = "Connection timeout";
            qWarning() << "[MCP Bridge] ⏱️" << errorMsg;
            break;
            
        case QAbstractSocket::NetworkError:
            errorMsg = "Network error";
            qWarning() << "[MCP Bridge] 🌐" << errorMsg;
            break;
            
        case QAbstractSocket::ConnectionRefusedError:
            errorMsg = "Connection refused";
            qWarning() << "[MCP Bridge] 🚫" << errorMsg;
            break;
            
        default:
            errorMsg = QString("WebSocket error code: %1").arg(error);
            qWarning() << "[MCP Bridge] ⚠️" << errorMsg;
            break;
    }
    
    Q_EMIT errorOccurred(errorMsg);
}

MCP::MCPResponse MCPBridgeWorker::processRequest(const MCP::MCPRequest& request) {
    // Check for built-in commands
    if (request.type == MCP::Commands::PING ||
        request.type == MCP::Commands::GET_VERSION ||
        request.type == MCP::Commands::GET_CAPABILITIES) {
        return handleBuiltinCommand(request);
    }
    
    // Lookup handler in registry
    IMCPCommandHandler* handler = MCPCommandRegistry::instance()->getHandler(request.type);
    
    if (!handler) {
        qWarning() << "[MCP Bridge] No handler for command type:" << request.type;
        return MCP::MCPResponse::notSupported(request.requestId, request.type);
    }
    
    if (!handler->isSupported()) {
        qWarning() << "[MCP Bridge] Handler for" << request.type << "is not currently supported";
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Command '%1' is not currently supported").arg(request.type)
        );
    }
    
    // Execute handler
    try {
        return handler->handle(request);
    } catch (const std::exception& e) {
        qCritical() << "[MCP Bridge] Exception in handler for" << request.type << ":" << e.what();
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Internal error: %1").arg(e.what())
        );
    } catch (...) {
        qCritical() << "[MCP Bridge] Unknown exception in handler for" << request.type;
        return MCP::MCPResponse::error(
            request.requestId,
            "Internal error: unknown exception"
        );
    }
}

MCP::MCPResponse MCPBridgeWorker::handleBuiltinCommand(const MCP::MCPRequest& request) {
    if (request.type == MCP::Commands::PING) {
        // Simple ping/pong
        QJsonObject data;
        data["pong"] = true;
        data["timestamp"] = QDateTime::currentSecsSinceEpoch();
        return MCP::MCPResponse::success(request.requestId, "pong", data);
        
    } else if (request.type == MCP::Commands::GET_VERSION) {
        // Return protocol version and client info
        QJsonObject data;
        data["protocol_version"] = MCP::PROTOCOL_VERSION;
        data["client_name"] = "AdaptixC2 Client";
        data["client_version"] = "0.9.0";  // TODO: Get from actual version
        return MCP::MCPResponse::success(request.requestId, "Version info", data);
        
    } else if (request.type == MCP::Commands::GET_CAPABILITIES) {
        // Return all capabilities
        QList<MCP::Capability> caps = MCPCommandRegistry::instance()->getAllCapabilities();
        
        QJsonArray capsArray;
        for (const auto& cap : caps) {
            capsArray.append(cap.toJson());
        }
        
        QJsonObject data;
        data["capabilities"] = capsArray;
        data["total"] = caps.size();
        
        return MCP::MCPResponse::success(request.requestId, 
                                        QString("%1 capabilities").arg(caps.size()), 
                                        data);
    }
    
    return MCP::MCPResponse::error(request.requestId, "Unknown built-in command");
}

bool MCPBridgeWorker::validateAuth(const MCP::MCPRequest& request) {
    // If no auth token is set, allow all requests
    if (authToken.isEmpty()) {
        return true;
    }
    
    // Check for auth token in params
    QString providedToken = request.params["auth_token"].toString();
    return providedToken == authToken;
}

void MCPBridgeWorker::logCommand(const MCP::MCPRequest& request, 
                                 const MCP::MCPResponse& response) {
    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString status = response.status;
    QString logLine = QString("[%1] MCP Command: %2 | ID: %3 | Status: %4")
                        .arg(timestamp)
                        .arg(request.type)
                        .arg(request.requestId)
                        .arg(status);
    
    if (status != MCP::Status::SUCCESS) {
        logLine += QString(" | Error: %1").arg(response.message);
    }
    
    qInfo().noquote() << logLine;
    
    // TODO: Could also write to file or database for persistent audit trail
}

void MCPBridgeWorker::sendResponse(const MCP::MCPResponse& response) {
    if (!mcpConnection || mcpConnection->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[MCP Bridge] Cannot send response: no active connection";
        return;
    }
    
    QJsonDocument doc(response.toJson());
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    
    mcpConnection->sendTextMessage(jsonStr);
    
    qDebug() << "[MCP Bridge] 📤 Sent response | ID:" << response.requestId 
             << "| Status:" << response.status;
}

