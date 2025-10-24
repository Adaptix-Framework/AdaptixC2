#include "InfoHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>
#include <Agent/Task.h>

InfoHandler::InfoHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

bool InfoHandler::isSupported() const {
    return adaptixWidget != nullptr;
}

QString InfoHandler::getDescription() const {
    return "Query client information (agents, listeners, tasks)";
}

MCP::MCPResponse InfoHandler::handle(const MCP::MCPRequest& request) {
    QString subCommand = request.params["command"].toString();
    
    if (subCommand == "list_agents") {
        return handleListAgents(request);
    } else if (subCommand == "list_listeners") {
        return handleListListeners(request);
    } else if (subCommand == "get_agent_info") {
        return handleGetAgentInfo(request);
    } else if (subCommand == "get_agent_console") {
        return handleGetAgentConsole(request);
    } else if (subCommand == "list_credentials") {
        return handleListCredentials(request);
    } else if (subCommand == "list_downloads") {
        return handleListDownloads(request);
    } else if (subCommand == "list_screenshots") {
        return handleListScreenshots(request);
    } else if (subCommand == "list_tasks") {
        return handleListTasks(request);
    } else if (subCommand == "get_task_output") {
        return handleGetTaskOutput(request);
    } else {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Unknown info command: %1").arg(subCommand)
        );
    }
}

MCP::MCPResponse InfoHandler::handleListAgents(const MCP::MCPRequest& request) {
    Q_UNUSED(request);
    
    QJsonArray agentsArray;
    
    // Iterate through all agents
    for (auto it = adaptixWidget->AgentsMap.constBegin(); 
         it != adaptixWidget->AgentsMap.constEnd(); ++it) {
        Agent* agent = it.value();
        if (!agent) continue;
        
        QJsonObject agentObj;
        agentObj["id"] = agent->data.Id;
        agentObj["name"] = agent->data.Name;
        agentObj["hostname"] = agent->data.Computer;
        agentObj["username"] = agent->data.Username;
        agentObj["domain"] = agent->data.Domain;
        agentObj["internal_ip"] = agent->data.InternalIP;
        agentObj["external_ip"] = agent->data.ExternalIP;
        agentObj["process_name"] = agent->data.Process;
        agentObj["process_id"] = agent->data.Pid;
        agentObj["arch"] = agent->data.Arch;
        agentObj["elevated"] = agent->data.Elevated;
        agentObj["os"] = agent->data.OsDesc;
        agentObj["os_code"] = agent->data.Os;
        agentObj["sleep"] = agent->data.Sleep;
        agentObj["jitter"] = agent->data.Jitter;
        agentObj["listener"] = agent->data.Listener;
        
        agentsArray.append(agentObj);
    }
    
    QJsonObject data;
    data["agents"] = agentsArray;
    data["total"] = agentsArray.size();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Found %1 agents").arg(agentsArray.size()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleListListeners(const MCP::MCPRequest& request) {
    Q_UNUSED(request);
    
    // TODO: Implement listener listing
    // Need to find the correct way to access listener data in AdaptixWidget
    
    QJsonArray listenersArray;
    
    QJsonObject data;
    data["listeners"] = listenersArray;
    data["total"] = 0;
    data["note"] = "Listener listing not yet implemented";
    
    return MCP::MCPResponse::success(
        request.requestId,
        "Listener listing not yet implemented",
        data
    );
}

MCP::MCPResponse InfoHandler::handleGetAgentInfo(const MCP::MCPRequest& request) {
    QString agentId = request.params["agent_id"].toString();
    
    if (agentId.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: agent_id");
    }
    
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Agent '%1' not found").arg(agentId)
        );
    }
    
    // Build detailed agent info
    QJsonObject agentInfo;
    agentInfo["id"] = agent->data.Id;
    agentInfo["name"] = agent->data.Name;
    agentInfo["hostname"] = agent->data.Computer;
    agentInfo["username"] = agent->data.Username;
    agentInfo["domain"] = agent->data.Domain;
    agentInfo["internal_ip"] = agent->data.InternalIP;
    agentInfo["external_ip"] = agent->data.ExternalIP;
    agentInfo["process_name"] = agent->data.Process;
    agentInfo["process_id"] = agent->data.Pid;
    agentInfo["thread_id"] = agent->data.Tid;
    agentInfo["arch"] = agent->data.Arch;
    agentInfo["elevated"] = agent->data.Elevated;
    agentInfo["os"] = agent->data.OsDesc;
    agentInfo["os_code"] = agent->data.Os;
    agentInfo["sleep"] = agent->data.Sleep;
    agentInfo["jitter"] = agent->data.Jitter;
    agentInfo["listener"] = agent->data.Listener;
    agentInfo["first_seen"] = agent->data.FirstOnlineTime;
    agentInfo["last_tick"] = agent->data.LastTick;
    agentInfo["tags"] = agent->data.Tags;
    agentInfo["mark"] = agent->data.Mark;
    agentInfo["impersonated"] = agent->data.Impersonated;
    
    QJsonObject data;
    data["agent"] = agentInfo;
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Agent info for %1").arg(agentId),
        data
    );
}

MCP::MCPResponse InfoHandler::handleGetAgentConsole(const MCP::MCPRequest& request) {
    QString agentId = request.params["agent_id"].toString();
    
    if (agentId.isEmpty()) {
        return MCP::MCPResponse::error(request.requestId, 
            "Missing required parameter: agent_id");
    }
    
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Agent '%1' not found").arg(agentId)
        );
    }
    
    if (!agent->Console) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Agent '%1' has no console").arg(agentId)
        );
    }
    
    // Get console output text
    QString consoleText = agent->Console->GetConsoleOutput();
    
    QJsonObject data;
    data["agent_id"] = agentId;
    data["console_output"] = consoleText;
    data["output_length"] = consoleText.length();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Console output for agent %1 (%2 chars)").arg(agentId).arg(consoleText.length()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleListCredentials(const MCP::MCPRequest& request) {
    Q_UNUSED(request);
    
    QJsonArray credsArray;
    
    // Iterate through all credentials
    for (const auto& cred : adaptixWidget->Credentials) {
        QJsonObject credObj;
        credObj["id"] = cred.CredId;
        credObj["username"] = cred.Username;
        credObj["password"] = cred.Password;
        credObj["realm"] = cred.Realm;
        credObj["type"] = cred.Type;
        credObj["tag"] = cred.Tag;
        credObj["date"] = cred.Date;
        credObj["storage"] = cred.Storage;
        credObj["agent_id"] = cred.AgentId;
        credObj["host"] = cred.Host;
        
        credsArray.append(credObj);
    }
    
    QJsonObject data;
    data["credentials"] = credsArray;
    data["total"] = credsArray.size();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Found %1 credentials").arg(credsArray.size()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleListDownloads(const MCP::MCPRequest& request) {
    Q_UNUSED(request);
    
    QJsonArray downloadsArray;
    
    // Iterate through all downloads
    for (auto it = adaptixWidget->Downloads.constBegin(); 
         it != adaptixWidget->Downloads.constEnd(); ++it) {
        const DownloadData& download = it.value();
        
        QJsonObject dlObj;
        dlObj["file_id"] = download.FileId;
        dlObj["agent_id"] = download.AgentId;
        dlObj["agent_name"] = download.AgentName;
        dlObj["user"] = download.User;
        dlObj["computer"] = download.Computer;
        dlObj["filename"] = download.Filename;
        dlObj["total_size"] = download.TotalSize;
        dlObj["received_size"] = download.RecvSize;
        dlObj["state"] = download.State;
        dlObj["date"] = download.Date;
        dlObj["progress_percent"] = download.TotalSize > 0 ? 
            (download.RecvSize * 100 / download.TotalSize) : 0;
        
        downloadsArray.append(dlObj);
    }
    
    QJsonObject data;
    data["downloads"] = downloadsArray;
    data["total"] = downloadsArray.size();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Found %1 downloads").arg(downloadsArray.size()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleListScreenshots(const MCP::MCPRequest& request) {
    Q_UNUSED(request);
    
    QJsonArray screenshotsArray;
    
    // Iterate through all screenshots
    for (auto it = adaptixWidget->Screenshots.constBegin(); 
         it != adaptixWidget->Screenshots.constEnd(); ++it) {
        const ScreenData& screen = it.value();
        
        QJsonObject screenObj;
        screenObj["screen_id"] = screen.ScreenId;
        screenObj["user"] = screen.User;
        screenObj["computer"] = screen.Computer;
        screenObj["date"] = screen.Date;
        screenObj["note"] = screen.Note;
        screenObj["content_size"] = screen.Content.size();
        // 不包含实际图片内容，避免数据过大
        
        screenshotsArray.append(screenObj);
    }
    
    QJsonObject data;
    data["screenshots"] = screenshotsArray;
    data["total"] = screenshotsArray.size();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Found %1 screenshots").arg(screenshotsArray.size()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleListTasks(const MCP::MCPRequest& request) {
    QString agentId = request.params["agent_id"].toString();
    
    QJsonArray tasksArray;
    
    // Iterate through all tasks
    for (auto it = adaptixWidget->TasksMap.constBegin(); 
         it != adaptixWidget->TasksMap.constEnd(); ++it) {
        Task* task = it.value();
        
        // If agent_id is specified, filter by agent
        if (!agentId.isEmpty() && task->data.AgentId != agentId) {
            continue;
        }
        
        QJsonObject taskObj;
        taskObj["task_id"] = task->data.TaskId;
        taskObj["agent_id"] = task->data.AgentId;
        taskObj["command"] = task->data.CommandLine;
        taskObj["status"] = task->data.Status;
        taskObj["completed"] = task->data.Completed;
        taskObj["start_time"] = task->data.StartTime;
        taskObj["finish_time"] = task->data.FinishTime;
        taskObj["computer"] = task->data.Computer;
        taskObj["user"] = task->data.User;
        
        // Include output preview (first 200 chars)
        QString outputPreview = task->data.Output;
        if (outputPreview.length() > 200) {
            outputPreview = outputPreview.left(200) + "...";
        }
        taskObj["output_preview"] = outputPreview;
        
        tasksArray.append(taskObj);
    }
    
    QJsonObject data;
    data["tasks"] = tasksArray;
    data["count"] = tasksArray.size();
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Found %1 tasks").arg(tasksArray.size()),
        data
    );
}

MCP::MCPResponse InfoHandler::handleGetTaskOutput(const MCP::MCPRequest& request) {
    QString taskId = request.params["task_id"].toString();
    
    if (taskId.isEmpty()) {
        return MCP::MCPResponse::error(
            request.requestId,
            "Missing required parameter: task_id"
        );
    }
    
    // Find the task
    Task* task = adaptixWidget->TasksMap.value(taskId, nullptr);
    
    if (!task) {
        return MCP::MCPResponse::error(
            request.requestId,
            QString("Task not found: %1").arg(taskId)
        );
    }
    
    QJsonObject data;
    data["task_id"] = task->data.TaskId;
    data["agent_id"] = task->data.AgentId;
    data["command"] = task->data.CommandLine;
    data["status"] = task->data.Status;
    data["completed"] = task->data.Completed;
    data["start_time"] = task->data.StartTime;
    data["finish_time"] = task->data.FinishTime;
    data["computer"] = task->data.Computer;
    data["user"] = task->data.User;
    data["output"] = task->data.Output;  // Full output
    
    return MCP::MCPResponse::success(
        request.requestId,
        QString("Retrieved task %1").arg(taskId),
        data
    );
}

