#ifndef MCPPROTOCOL_H
#define MCPPROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

/**
 * MCP Bridge Protocol Definitions
 * 
 * This file defines the communication protocol between the MCP Server (Go)
 * and the AdaptixClient MCP Bridge.
 * 
 * Protocol Version: 1.0
 */

namespace MCP {

// Protocol version
const QString PROTOCOL_VERSION = "1.0";

// Command types (extensible)
namespace Commands {
    const QString EXECUTE_BOF = "execute_bof";
    const QString EXECUTE_AXSCRIPT = "execute_axscript";
    const QString GET_CAPABILITIES = "get_capabilities";
    const QString QUERY_EXTENDER = "query_extender";
    const QString PING = "ping";
    const QString GET_VERSION = "get_version";
}

// Response status codes
namespace Status {
    const QString SUCCESS = "success";
    const QString ERROR = "error";
    const QString NOT_SUPPORTED = "not_supported";
    const QString INVALID_PARAMS = "invalid_params";
    const QString TIMEOUT = "timeout";
}

/**
 * MCPRequest - Request structure from MCP Server
 */
struct MCPRequest {
    QString version;           // Protocol version (e.g., "1.0")
    QString type;              // Command type (e.g., "execute_bof")
    QString requestId;         // Unique request ID for async tracking
    QJsonObject params;        // Command parameters
    
    /**
     * Parse from JSON object
     */
    static MCPRequest fromJson(const QJsonObject& json) {
        MCPRequest req;
        req.version = json["version"].toString(PROTOCOL_VERSION);
        req.type = json["type"].toString();
        req.requestId = json["request_id"].toString();
        req.params = json["params"].toObject();
        return req;
    }
    
    /**
     * Convert to JSON object
     */
    QJsonObject toJson() const {
        QJsonObject json;
        json["version"] = version;
        json["type"] = type;
        json["request_id"] = requestId;
        json["params"] = params;
        return json;
    }
    
    /**
     * Validate request
     */
    bool isValid() const {
        return !type.isEmpty() && !requestId.isEmpty();
    }
};

/**
 * MCPResponse - Response structure to MCP Server
 */
struct MCPResponse {
    QString version;           // Protocol version
    QString requestId;         // Matching request ID
    QString status;            // Status code (success/error/etc.)
    QString message;           // Human-readable message
    QJsonObject data;          // Response data
    
    /**
     * Create a success response
     */
    static MCPResponse success(const QString& reqId, const QString& msg = "", 
                              const QJsonObject& data = QJsonObject()) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::SUCCESS;
        resp.message = msg;
        resp.data = data;
        return resp;
    }
    
    /**
     * Create an error response
     */
    static MCPResponse error(const QString& reqId, const QString& msg) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::ERROR;
        resp.message = msg;
        return resp;
    }
    
    /**
     * Create a not-supported response
     */
    static MCPResponse notSupported(const QString& reqId, const QString& type) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::NOT_SUPPORTED;
        resp.message = QString("Command type '%1' not supported").arg(type);
        return resp;
    }
    
    /**
     * Convert to JSON object
     */
    QJsonObject toJson() const {
        QJsonObject json;
        json["version"] = version;
        json["request_id"] = requestId;
        json["status"] = status;
        json["message"] = message;
        if (!data.isEmpty()) {
            json["data"] = data;
        }
        return json;
    }
};

/**
 * Capability - Describes a supported capability
 */
struct Capability {
    QString name;              // Capability name
    QString version;           // Version
    QString description;       // Description
    bool available;            // Is available
    QJsonObject metadata;      // Additional metadata
    
    QJsonObject toJson() const {
        QJsonObject json;
        json["name"] = name;
        json["version"] = version;
        json["description"] = description;
        json["available"] = available;
        if (!metadata.isEmpty()) {
            json["metadata"] = metadata;
        }
        return json;
    }
};

} // namespace MCP

#endif // MCPPROTOCOL_H

