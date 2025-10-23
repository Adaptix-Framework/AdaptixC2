#include "ListenerHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <QJsonArray>
#include <QJsonDocument>

using namespace MCP;

ListenerHandler::ListenerHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse ListenerHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "list") {
        return handleListListeners(request);
    } else if (command == "get_info") {
        return handleGetListenerInfo(request);
    } else if (command == "create") {
        return handleCreateListener(request);
    } else if (command == "edit") {
        return handleEditListener(request);
    } else if (command == "stop") {
        return handleStopListener(request);
    } else if (command == "list_extenders") {
        return handleListExtenders(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown listener command: %1").arg(command)
        );
    }
}

MCPResponse ListenerHandler::handleListListeners(const MCPRequest& request)
{
    Q_UNUSED(request);
    
    QJsonArray listenersArray;
    
    // Iterate through all listeners
    for (const ListenerData& listener : adaptixWidget->Listeners) {
        QJsonObject listenerObj;
        listenerObj["name"] = listener.Name;
        listenerObj["reg_name"] = listener.ListenerRegName;
        listenerObj["type"] = listener.ListenerType;
        listenerObj["protocol"] = listener.ListenerProtocol;
        listenerObj["bind_host"] = listener.BindHost;
        listenerObj["bind_port"] = listener.BindPort;
        listenerObj["agent_addresses"] = listener.AgentAddresses;
        listenerObj["status"] = listener.Status;
        
        listenersArray.append(listenerObj);
    }
    
    QJsonObject data;
    data["listeners"] = listenersArray;
    data["total"] = listenersArray.count();
    
    return MCPResponse::success(
        request.requestId,
        QString("Found %1 listeners").arg(listenersArray.count()),
        data
    );
}

MCPResponse ListenerHandler::handleGetListenerInfo(const MCPRequest& request)
{
    QString listenerName = request.params.value("name").toString();
    
    if (listenerName.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: name"
        );
    }
    
    // Find the listener
    for (const ListenerData& listener : adaptixWidget->Listeners) {
        if (listener.Name == listenerName) {
            QJsonObject listenerObj;
            listenerObj["name"] = listener.Name;
            listenerObj["reg_name"] = listener.ListenerRegName;
            listenerObj["type"] = listener.ListenerType;
            listenerObj["protocol"] = listener.ListenerProtocol;
            listenerObj["bind_host"] = listener.BindHost;
            listenerObj["bind_port"] = listener.BindPort;
            listenerObj["agent_addresses"] = listener.AgentAddresses;
            listenerObj["status"] = listener.Status;
            listenerObj["config"] = listener.Data; // Full config JSON
            
            QJsonObject data;
            data["listener"] = listenerObj;
            
            return MCPResponse::success(
                request.requestId,
                QString("Listener info for %1").arg(listenerName),
                data
            );
        }
    }
    
    return MCPResponse::error(
        request.requestId,
        QString("Listener not found: %1").arg(listenerName)
    );
}

MCPResponse ListenerHandler::handleCreateListener(const MCPRequest& request)
{
    QString name = request.params.value("name").toString();
    QString listenerType = request.params.value("listener_type").toString();
    QString config = request.params.value("config").toString();
    
    if (name.isEmpty() || listenerType.isEmpty() || config.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameters: name, listener_type, config"
        );
    }
    
    // Validate JSON config
    QJsonParseError parseError;
    QJsonDocument::fromJson(config.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return MCPResponse::error(
            request.requestId,
            QString("Invalid JSON config: %1").arg(parseError.errorString())
        );
    }
    
    // Check if listener already exists
    for (const ListenerData& listener : adaptixWidget->Listeners) {
        if (listener.Name == name) {
            return MCPResponse::error(
                request.requestId,
                QString("Listener already exists: %1").arg(name)
            );
        }
    }
    
    // Get AuthProfile
    if (!adaptixWidget || !adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqListenerStart(
        name,
        listenerType,
        config,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["listener_name"] = name;
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse ListenerHandler::handleEditListener(const MCPRequest& request)
{
    QString name = request.params.value("name").toString();
    QString listenerType = request.params.value("listener_type").toString();
    QString config = request.params.value("config").toString();
    
    if (name.isEmpty() || listenerType.isEmpty() || config.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameters: name, listener_type, config"
        );
    }
    
    // Validate JSON config
    QJsonParseError parseError;
    QJsonDocument::fromJson(config.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return MCPResponse::error(
            request.requestId,
            QString("Invalid JSON config: %1").arg(parseError.errorString())
        );
    }
    
    // Check if listener exists
    bool found = false;
    for (const ListenerData& listener : adaptixWidget->Listeners) {
        if (listener.Name == name) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        return MCPResponse::error(
            request.requestId,
            QString("Listener not found: %1").arg(name)
        );
    }
    
    // Get AuthProfile
    if (!adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqListenerEdit(
        name,
        listenerType,
        config,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["listener_name"] = name;
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse ListenerHandler::handleStopListener(const MCPRequest& request)
{
    QString name = request.params.value("name").toString();
    
    if (name.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: name"
        );
    }
    
    // Find listener to get its type
    QString listenerType;
    bool found = false;
    for (const ListenerData& listener : adaptixWidget->Listeners) {
        if (listener.Name == name) {
            found = true;
            listenerType = listener.ListenerRegName; // Use reg_name as type
            break;
        }
    }
    
    if (!found) {
        return MCPResponse::error(
            request.requestId,
            QString("Listener not found: %1").arg(name)
        );
    }
    
    // Get AuthProfile
    if (!adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqListenerStop(
        name,
        listenerType,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["listener_name"] = name;
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse ListenerHandler::handleListExtenders(const MCPRequest& request)
{
    Q_UNUSED(request);
    
    QJsonArray extendersArray;
    
    if (adaptixWidget && adaptixWidget->ScriptManager) {
        // Get all registered listener extenders
        QList<QString> listenersList = adaptixWidget->ScriptManager->ListenerScriptList();
        
        for (const QString& listenerName : listenersList) {
            QJsonObject extenderObj;
            extenderObj["name"] = listenerName;
            extenderObj["type"] = "listener";
            
            extendersArray.append(extenderObj);
        }
    }
    
    QJsonObject data;
    data["extenders"] = extendersArray;
    data["total"] = extendersArray.count();
    
    return MCPResponse::success(
        request.requestId,
        QString("Found %1 listener extenders").arg(extendersArray.count()),
        data
    );
}
