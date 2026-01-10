#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/ScreenshotsWidget.h>
#include <UI/Widgets/TunnelsWidget.h>
#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/AxConsoleWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogSyncPacket.h>

namespace {

    QIcon getTargetOsIcon(int os, bool owned, bool alive) {
        switch (os) {
            case OS_WINDOWS:
                if (owned) return QIcon(":/icons/os_win_red");
                if (alive) return QIcon(":/icons/os_win_blue");
                return QIcon(":/icons/os_win_grey");
            case OS_LINUX:
                if (owned) return QIcon(":/icons/os_linux_red");
                if (alive) return QIcon(":/icons/os_linux_blue");
                return QIcon(":/icons/os_linux_grey");
            case OS_MAC:
                if (owned) return QIcon(":/icons/os_mac_red");
                if (alive) return QIcon(":/icons/os_mac_blue");
                return QIcon(":/icons/os_mac_grey");
            default:
                return QIcon();
        }
    }

    ListenerData parseListenerData(const QJsonObject &json) {
        ListenerData data = {};
        data.Name             = json["l_name"].toString();
        data.ListenerRegName  = json["l_reg_name"].toString();
        data.ListenerType     = json["l_type"].toString();
        data.ListenerProtocol = json["l_protocol"].toString();
        data.BindHost         = json["l_bind_host"].toString();
        data.BindPort         = json["l_bind_port"].toString();
        data.AgentAddresses   = json["l_agent_addr"].toString();
        data.DateTimestamp    = static_cast<qint64>(json["l_create_time"].toDouble());
        data.Date             = UnixTimestampGlobalToStringLocalSmall(data.DateTimestamp);
        data.Status           = json["l_status"].toString();
        data.Data             = json["l_data"].toString();
        return data;
    }

    TaskData parseTaskData(const QJsonObject &json) {
        TaskData data = {};
        data.TaskId      = json["a_task_id"].toString();
        data.TaskType    = json["a_task_type"].toDouble();
        data.AgentId     = json["a_id"].toString();
        data.StartTime   = json["a_start_time"].toDouble();
        data.CommandLine = json["a_cmdline"].toString();
        data.Client      = json["a_client"].toString();
        data.User        = json["a_user"].toString();
        data.Computer    = json["a_computer"].toString();
        data.FinishTime  = json["a_finish_time"].toDouble();
        data.MessageType = json["a_msg_type"].toDouble();
        data.Message     = json["a_message"].toString();
        data.Output      = json["a_text"].toString();
        data.Completed   = json["a_completed"].toBool();
        return data;
    }

    DownloadData parseDownloadData(const QJsonObject &json) {
        DownloadData data = {};
        data.AgentId       = json["d_agent_id"].toString();
        data.FileId        = json["d_file_id"].toString();
        data.AgentName     = json["d_agent_name"].toString();
        data.User          = json["d_user"].toString();
        data.Computer      = json["d_computer"].toString();
        data.Filename      = json["d_file"].toString();
        data.TotalSize     = json["d_size"].toDouble();
        data.DateTimestamp = static_cast<qint64>(json["d_date"].toDouble());
        data.Date          = UnixTimestampGlobalToStringLocal(data.DateTimestamp);
        data.RecvSize      = 0;
        data.State         = DOWNLOAD_STATE_RUNNING;
        return data;
    }

    ScreenData parseScreenData(const QJsonObject &json) {
        ScreenData data = {};
        data.ScreenId      = json["s_screen_id"].toString();
        data.User          = json["s_user"].toString();
        data.Computer      = json["s_computer"].toString();
        data.Note          = json["s_note"].toString();
        data.DateTimestamp = static_cast<qint64>(json["s_date"].toDouble());
        data.Date          = UnixTimestampGlobalToStringLocal(data.DateTimestamp);
        data.Content       = QByteArray::fromBase64(json["s_content"].toString().toUtf8());
        return data;
    }

    TunnelData parseTunnelData(const QJsonObject &json) {
        TunnelData data = {};
        data.TunnelId  = json["p_tunnel_id"].toString();
        data.AgentId   = json["p_agent_id"].toString();
        data.Computer  = json["p_computer"].toString();
        data.Username  = json["p_username"].toString();
        data.Process   = json["p_process"].toString();
        data.Type      = json["p_type"].toString();
        data.Info      = json["p_info"].toString();
        data.Interface = json["p_interface"].toString();
        data.Port      = json["p_port"].toString();
        data.Client    = json["p_client"].toString();
        data.Fhost     = json["p_fhost"].toString();
        data.Fport     = json["p_fport"].toString();
        return data;
    }

    PivotData parsePivotData(const QJsonObject &json) {
        PivotData data = {};
        data.PivotId       = json["p_pivot_id"].toString();
        data.PivotName     = json["p_pivot_name"].toString();
        data.ParentAgentId = json["p_parent_agent_id"].toString();
        data.ChildAgentId  = json["p_child_agent_id"].toString();
        return data;
    }

}

bool AdaptixWidget::isValidSyncPacket(QJsonObject jsonObj)
{
    if (!jsonObj.contains("type") || !jsonObj["type"].isDouble()) {
        qWarning() << "[SyncPacket] Invalid packet: missing or invalid 'type' field";
        return false;
    }

    int spType = jsonObj["type"].toDouble();

    auto checkField = [&jsonObj](const char* field, auto checker) -> bool {
        if (!jsonObj.contains(field) || !checker(jsonObj[field])) {
            qWarning() << "[SyncPacket] Missing or invalid field:" << field;
            return false;
        }
        return true;
    };

    auto isStr = [](const QJsonValue& v) { return v.isString(); };
    auto isNum = [](const QJsonValue& v) { return v.isDouble(); };
    auto isArr = [](const QJsonValue& v) { return v.isArray(); };
    auto isBl  = [](const QJsonValue& v) { return v.isBool(); };

    switch (spType) {

    case TYPE_SYNC_START:
        return checkField("count", isNum) && checkField("interfaces", isArr);

    case TYPE_SYNC_FINISH:
        return true;

    case TYPE_SYNC_BATCH:
        return checkField("packets", isArr);

    case TYPE_SYNC_CATEGORY_BATCH:
        return checkField("category", isStr) &&
               checkField("packets", isArr);

    case SP_TYPE_EVENT:
        return checkField("event_type", isNum) &&
               checkField("date", isNum) &&
               checkField("message", isStr);

    case TYPE_LISTENER_REG:
        return checkField("l_name", isStr) &&
               checkField("l_protocol", isStr) &&
               checkField("l_type", isStr) &&
               checkField("ax", isStr);

    case TYPE_LISTENER_START:
    case TYPE_LISTENER_EDIT:
        return checkField("l_name", isStr) &&
               checkField("l_reg_name", isStr) &&
               checkField("l_protocol", isStr) &&
               checkField("l_type", isStr) &&
               checkField("l_bind_host", isStr) &&
               checkField("l_bind_port", isStr) &&
               checkField("l_agent_addr", isStr) &&
               checkField("l_create_time", isNum) &&
               checkField("l_status", isStr) &&
               checkField("l_data", isStr);

    case TYPE_LISTENER_STOP:
        return checkField("l_name", isStr);

    case TYPE_AGENT_REG:
        return checkField("agent", isStr) &&
               checkField("ax", isStr) &&
               checkField("listeners", isArr);

    case TYPE_AGENT_NEW:
        return checkField("a_id", isStr) &&
               checkField("a_name", isStr) &&
               checkField("a_listener", isStr) &&
               checkField("a_async", isBl) &&
               checkField("a_external_ip", isStr) &&
               checkField("a_internal_ip", isStr) &&
               checkField("a_gmt_offset", isNum) &&
               checkField("a_acp", isNum) &&
               checkField("a_oemcp", isNum) &&
               checkField("a_sleep", isNum) &&
               checkField("a_jitter", isNum) &&
               checkField("a_pid", isStr) &&
               checkField("a_tid", isStr) &&
               checkField("a_arch", isStr) &&
               checkField("a_elevated", isBl) &&
               checkField("a_process", isStr) &&
               checkField("a_os", isNum) &&
               checkField("a_os_desc", isStr) &&
               checkField("a_domain", isStr) &&
               checkField("a_computer", isStr) &&
               checkField("a_username", isStr) &&
               checkField("a_impersonated", isStr) &&
               checkField("a_tags", isStr) &&
               checkField("a_mark", isStr) &&
               checkField("a_color", isStr) &&
               checkField("a_create_time", isNum) &&
               checkField("a_last_tick", isNum);

    case TYPE_AGENT_TICK:
        return checkField("a_id", isArr);

    case TYPE_AGENT_UPDATE:
        return checkField("a_id", isStr);

    case TYPE_AGENT_REMOVE:
        return checkField("a_id", isStr);

    case TYPE_AGENT_TASK_SYNC:
        return checkField("a_id", isStr) &&
               checkField("a_task_id", isStr) &&
               checkField("a_task_type", isNum) &&
               checkField("a_start_time", isNum) &&
               checkField("a_cmdline", isStr) &&
               checkField("a_client", isStr) &&
               checkField("a_user", isStr) &&
               checkField("a_computer", isStr) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_TASK_UPDATE:
        return checkField("a_id", isStr) &&
               checkField("a_task_id", isStr) &&
               checkField("a_task_type", isNum) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_TASK_SEND:
        return checkField("a_task_id", isArr);

    case TYPE_AGENT_TASK_REMOVE:
        return checkField("a_task_id", isStr);

    case TYPE_AGENT_TASK_HOOK:
        return checkField("a_id", isStr) &&
               checkField("a_task_id", isStr) &&
               checkField("a_hook_id", isStr) &&
               checkField("a_job_index", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_CONSOLE_OUT:
        return checkField("time", isNum) &&
               checkField("a_id", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_message", isStr) &&
               checkField("a_msg_type", isNum);

    case TYPE_AGENT_CONSOLE_TASK_SYNC:
        return checkField("a_id", isStr) &&
               checkField("a_task_id", isStr) &&
               checkField("a_start_time", isNum) &&
               checkField("a_cmdline", isStr) &&
               checkField("a_client", isStr) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_CONSOLE_TASK_UPD:
        return checkField("a_id", isStr) &&
               checkField("a_task_id", isStr) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_CHAT_MESSAGE:
        return checkField("c_username", isStr) &&
               checkField("c_message", isStr) &&
               checkField("c_date", isNum);

    case TYPE_DOWNLOAD_CREATE:
        return checkField("d_agent_id", isStr) &&
               checkField("d_file_id", isStr) &&
               checkField("d_agent_name", isStr) &&
               checkField("d_user", isStr) &&
               checkField("d_computer", isStr) &&
               checkField("d_file", isStr) &&
               checkField("d_size", isNum) &&
               checkField("d_date", isNum);

    case TYPE_DOWNLOAD_UPDATE:
        return checkField("d_file_id", isStr) &&
               checkField("d_recv_size", isNum) &&
               checkField("d_state", isNum);

    case TYPE_DOWNLOAD_DELETE:
            return checkField("d_files_id", isArr);

    case TYPE_TUNNEL_CREATE:
        return checkField("p_tunnel_id", isStr) &&
               checkField("p_agent_id", isStr) &&
               checkField("p_computer", isStr) &&
               checkField("p_username", isStr) &&
               checkField("p_process", isStr) &&
               checkField("p_type", isStr) &&
               checkField("p_info", isStr) &&
               checkField("p_interface", isStr) &&
               checkField("p_port", isStr) &&
               checkField("p_client", isStr) &&
               checkField("p_fport", isStr) &&
               checkField("p_fhost", isStr);

    case TYPE_TUNNEL_EDIT:
        return checkField("p_tunnel_id", isStr) &&
               checkField("p_info", isStr);

    case TYPE_TUNNEL_DELETE:
        return checkField("p_tunnel_id", isStr);

    case TYPE_SCREEN_CREATE:
        return checkField("s_screen_id", isStr) &&
               checkField("s_user", isStr) &&
               checkField("s_computer", isStr) &&
               checkField("s_note", isStr) &&
               checkField("s_date", isNum) &&
               checkField("s_content", isStr);

    case TYPE_SCREEN_UPDATE:
        return checkField("s_screen_id", isStr) &&
               checkField("s_note", isStr);

    case TYPE_SCREEN_DELETE:
        return checkField("s_screen_id", isStr);

    case TYPE_CREDS_CREATE:
        return checkField("c_creds", isArr);

    case TYPE_CREDS_EDIT:
        return checkField("c_creds_id", isStr) &&
               checkField("c_username", isStr) &&
               checkField("c_password", isStr) &&
               checkField("c_realm", isStr) &&
               checkField("c_type", isStr) &&
               checkField("c_tag", isStr) &&
               checkField("c_storage", isStr) &&
               checkField("c_host", isStr);

    case TYPE_CREDS_DELETE:
        return checkField("c_creds_id", isArr);

    case TYPE_CREDS_SET_TAG:
        return checkField("c_creds_id", isArr) &&
               checkField("c_tag", isStr);

    case TYPE_TARGETS_CREATE:
        return checkField("t_targets", isArr);

    case TYPE_TARGETS_EDIT:
        return checkField("t_target_id", isStr) &&
               checkField("t_computer", isStr) &&
               checkField("t_domain", isStr) &&
               checkField("t_address", isStr) &&
               checkField("t_os", isNum) &&
               checkField("t_os_desk", isStr) &&
               checkField("t_tag", isStr) &&
               checkField("t_info", isStr) &&
               checkField("t_date", isNum) &&
               checkField("t_alive", isBl) &&
               jsonObj.contains("t_agents");

    case TYPE_TARGETS_DELETE:
        return checkField("t_target_id", isArr);

    case TYPE_TARGETS_SET_TAG:
        return checkField("t_targets_id", isArr) &&
               checkField("t_tag", isStr);

    case TYPE_BROWSER_DISKS:
        return checkField("b_agent_id", isStr) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_data", isStr);

    case TYPE_BROWSER_FILES:
        return checkField("b_agent_id", isStr) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_path", isStr) &&
               checkField("b_data", isStr);

    case TYPE_BROWSER_STATUS:
        return checkField("b_agent_id", isStr) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr);

    case TYPE_BROWSER_PROCESS:
        return checkField("b_agent_id", isStr) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_data", isStr);

    case TYPE_PIVOT_CREATE:
        return checkField("p_pivot_id", isStr) &&
               checkField("p_pivot_name", isStr) &&
               checkField("p_parent_agent_id", isStr) &&
               checkField("p_child_agent_id", isStr);

    case TYPE_PIVOT_DELETE:
        return checkField("p_pivot_id", isStr);

    default:
        qWarning() << "[SyncPacket] Unknown packet type:" << spType;
        return false;
    }
}

void AdaptixWidget::processSyncPacket(QJsonObject jsonObj)
{
    int spType = jsonObj["type"].toDouble();

    if (spType == TYPE_SYNC_BATCH || spType == TYPE_SYNC_CATEGORY_BATCH) {
        QJsonArray packetsArray = jsonObj["packets"].toArray();
        for (const QJsonValue &packetValue : packetsArray) {
            if (packetValue.isObject())
                processSyncPacket(packetValue.toObject());
        }
        if (this->sync && dialogSyncPacket) {
            dialogSyncPacket->receivedLogs += packetsArray.size();
            dialogSyncPacket->upgrade();
        }
        return;
    }

    if (this->sync && dialogSyncPacket && spType != TYPE_SYNC_BATCH) {
        dialogSyncPacket->receivedLogs++;
        dialogSyncPacket->upgrade();
    }

    switch (spType) {

    case TYPE_SYNC_START: {
        int count = jsonObj["count"].toDouble();
        QJsonArray interfaces = jsonObj["interfaces"].toArray();
        this->addresses.clear();
        for (const QJsonValue &addrValue : interfaces)
            this->addresses.append(addrValue.toString());
        this->sync = true;
        this->setSyncUpdateUI(false);
        dialogSyncPacket->init(count);
        break;
    }

    case TYPE_SYNC_FINISH:
        if (dialogSyncPacket) {
            this->sync = false;
            dialogSyncPacket->finish();
            QTimer::singleShot(100, this, [this]() {
                this->setSyncUpdateUI(true);
                Q_EMIT this->SyncedSignal();
            });
        }
        break;

    case TYPE_LISTENER_START:
        ListenersDock->AddListenerItem(parseListenerData(jsonObj));
        break;

    case TYPE_LISTENER_EDIT:
        ListenersDock->EditListenerItem(parseListenerData(jsonObj));
        break;

    case TYPE_LISTENER_STOP:
        ListenersDock->RemoveListenerItem(jsonObj["l_name"].toString());
        break;

    case TYPE_LISTENER_REG:
        this->RegisterListenerConfig(
            jsonObj["l_name"].toString(),
            jsonObj["l_protocol"].toString(),
            jsonObj["l_type"].toString(),
            jsonObj["ax"].toString()
        );
        break;

    case TYPE_AGENT_NEW: {
        Agent* newAgent = new Agent(jsonObj, this);
        SessionsTableDock->AddAgentItem(newAgent);
        SessionsGraphDock->AddAgent(newAgent, this->synchronized);
        if (synchronized)
            Q_EMIT eventNewAgent(newAgent->data.Id);
        break;
    }

    case TYPE_AGENT_UPDATE: {
        QString agentId = jsonObj["a_id"].toString();
        Agent* agent = AgentsMap.value(agentId, nullptr);
        if (agent) {
            auto oldData = agent->data;
            agent->Update(jsonObj);
            SessionsTableDock->UpdateAgentItem(oldData, agent);
        }
        break;
    }

    case TYPE_AGENT_TICK: {
        QJsonArray agentIDs = jsonObj["a_id"].toArray();
        for (const QJsonValue &idValue : agentIDs) {
            Agent* agent = AgentsMap.value(idValue.toString(), nullptr);
            if (agent) {
                agent->data.LastTick = QDateTime::currentSecsSinceEpoch();
                if (agent->data.Mark != "Terminated")
                    agent->MarkItem("");
            }
        }
        break;
    }

    case TYPE_AGENT_REMOVE: {
        QString agentId = jsonObj["a_id"].toString();
        if (this->AgentsMap.contains(agentId)) {
            SessionsGraphDock->RemoveAgent(this->AgentsMap[agentId], this->synchronized);
            SessionsTableDock->RemoveAgentItem(agentId);
            TasksDock->RemoveAgentTasksItem(agentId);
        }
        break;
    }

    case TYPE_AGENT_REG: {
        QStringList listeners;
        for (const QJsonValue &listener : jsonObj["listeners"].toArray())
            listeners.append(listener.toString());
        this->RegisterAgentConfig(jsonObj["agent"].toString(), jsonObj["ax"].toString(), listeners);
        break;
    }

    case TYPE_AGENT_TASK_SYNC:
        TasksDock->AddTaskItem(parseTaskData(jsonObj));
        break;

    case TYPE_AGENT_TASK_UPDATE: {
        QString taskId = jsonObj["a_task_id"].toString();
        if (TasksMap.contains(taskId)) {
            TaskData* task = &TasksMap[taskId];
            task->Completed = jsonObj["a_completed"].toBool();
            if (task->Completed) {
                task->FinishTime = jsonObj["a_finish_time"].toDouble();
                task->MessageType = jsonObj["a_msg_type"].toDouble();
                if (task->MessageType == CONSOLE_OUT_ERROR || task->MessageType == CONSOLE_OUT_LOCAL_ERROR)
                    task->Status = "Error";
                else if (task->MessageType == CONSOLE_OUT_INFO || task->MessageType == CONSOLE_OUT_LOCAL_INFO)
                    task->Status = "Canceled";
                else
                    task->Status = "Success";
            }
            if (task->Message.isEmpty())
                task->Message = jsonObj["a_message"].toString();
            task->Output += jsonObj["a_text"].toString();
            TasksDock->UpdateTaskItem(taskId, *task);

            if ( task->Completed && jsonObj.contains("a_handler_id") && jsonObj["a_handler_id"].isString() ) {
                QString handlerId = jsonObj["a_handler_id"].toString();
                if (!handlerId.isEmpty() && PostHandlersJS.contains(handlerId))
                    this->PostHandlerProcess(handlerId, *task);
            }
        }
        break;
    }

    case TYPE_AGENT_TASK_SEND: {
        for (const QJsonValue &idValue : jsonObj["a_task_id"].toArray()) {
            QString id = idValue.toString();
            if (TasksMap.contains(id))
                TasksMap[id].Status = "Running";
        }
        break;
    }

    case TYPE_AGENT_TASK_REMOVE:
        TasksDock->RemoveTaskItem(jsonObj["a_task_id"].toString());
        break;

    case TYPE_AGENT_TASK_HOOK:
        this->PostHookProcess(jsonObj);
        break;

    case TYPE_AGENT_CONSOLE_OUT: {
        QString agentId = jsonObj["a_id"].toString();
        if (AgentsMap.contains(agentId)) {
            AgentsMap[agentId]->Console->ConsoleOutputMessage(
                static_cast<qint64>(jsonObj["time"].toDouble()), "",
                jsonObj["a_msg_type"].toDouble(),
                jsonObj["a_message"].toString(),
                jsonObj["a_text"].toString(), false
            );
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_TASK_SYNC: {
        QString agentId = jsonObj["a_id"].toString();
        if (AgentsMap.contains(agentId)) {
            qint64 startTime = jsonObj["a_start_time"].toDouble();
            qint64 finishTime = jsonObj["a_finish_time"].toDouble();
            bool completed = jsonObj["a_completed"].toBool();
            AgentsMap[agentId]->Console->ConsoleOutputPrompt(
                startTime, jsonObj["a_task_id"].toString(),
                jsonObj["a_client"].toString(), jsonObj["a_cmdline"].toString()
            );
            if (!this->synchronized)
                AgentsMap[agentId]->Console->AddToHistory(jsonObj["a_cmdline"].toString());
            AgentsMap[agentId]->Console->ConsoleOutputMessage(
                completed ? finishTime : startTime,
                jsonObj["a_task_id"].toString(),
                jsonObj["a_msg_type"].toDouble(),
                jsonObj["a_message"].toString(),
                jsonObj["a_text"].toString(), completed
            );
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_TASK_UPD: {
        QString agentId = jsonObj["a_id"].toString();
        if (AgentsMap.contains(agentId)) {
            AgentsMap[agentId]->Console->ConsoleOutputMessage(
                jsonObj["a_finish_time"].toDouble(),
                jsonObj["a_task_id"].toString(),
                jsonObj["a_msg_type"].toDouble(),
                jsonObj["a_message"].toString(),
                jsonObj["a_text"].toString(),
                jsonObj["a_completed"].toBool()
            );
        }
        break;
    }

    case TYPE_CHAT_MESSAGE:
        ChatDock->AddChatMessage(
            jsonObj["c_date"].toDouble(),
            jsonObj["c_username"].toString(),
            jsonObj["c_message"].toString()
        );
        break;

    case TYPE_DOWNLOAD_CREATE:
        DownloadsDock->AddDownloadItem(parseDownloadData(jsonObj));
        break;

    case TYPE_DOWNLOAD_UPDATE:
        DownloadsDock->EditDownloadItem(
            jsonObj["d_file_id"].toString(),
            jsonObj["d_recv_size"].toDouble(),
            jsonObj["d_state"].toDouble()
        );
        break;

        case TYPE_DOWNLOAD_DELETE: {
            QStringList ids;
            for (const QJsonValue &val : jsonObj["d_files_id"].toArray())
                if (val.isString()) ids.append(val.toString());
            DownloadsDock->RemoveDownloadItem(ids);
            break;
        }

    case TYPE_SCREEN_CREATE:
        ScreenshotsDock->AddScreenshotItem(parseScreenData(jsonObj));
        break;

    case TYPE_SCREEN_UPDATE:
        ScreenshotsDock->EditScreenshotItem(
            jsonObj["s_screen_id"].toString(),
            jsonObj["s_note"].toString()
        );
        break;

    case TYPE_SCREEN_DELETE:
        ScreenshotsDock->RemoveScreenshotItem(jsonObj["s_screen_id"].toString());
        break;

    case TYPE_CREDS_CREATE: {
        QList<CredentialData> credList;
        for (const QJsonValue &val : jsonObj["c_creds"].toArray()) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();
            if (!obj.contains("c_creds_id") || !obj["c_creds_id"].isString()) continue;
            if (!obj.contains("c_username") || !obj["c_username"].isString()) continue;
            if (!obj.contains("c_password") || !obj["c_password"].isString()) continue;
            if (!obj.contains("c_realm")    || !obj["c_realm"].isString())    continue;
            if (!obj.contains("c_type")     || !obj["c_type"].isString())     continue;
            if (!obj.contains("c_tag")      || !obj["c_tag"].isString())      continue;
            if (!obj.contains("c_date")     || !obj["c_date"].isDouble())     continue;
            if (!obj.contains("c_storage")  || !obj["c_storage"].isString())  continue;
            if (!obj.contains("c_agent_id") || !obj["c_agent_id"].isString()) continue;
            if (!obj.contains("c_host")     || !obj["c_host"].isString())     continue;

            CredentialData c;
            c.CredId        = obj["c_creds_id"].toString();
            c.Username      = obj["c_username"].toString();
            c.Password      = obj["c_password"].toString();
            c.Realm         = obj["c_realm"].toString();
            c.Type          = obj["c_type"].toString();
            c.Tag           = obj["c_tag"].toString();
            c.DateTimestamp = static_cast<qint64>(obj["c_date"].toDouble());
            c.Date          = UnixTimestampGlobalToStringLocal(c.DateTimestamp);
            c.Storage       = obj["c_storage"].toString();
            c.AgentId       = obj["c_agent_id"].toString();
            c.Host          = obj["c_host"].toString();
            credList.append(c);
        }
        CredentialsDock->AddCredentialsItems(credList);
        break;
    }

    case TYPE_CREDS_EDIT: {
        CredentialData c = {};
        c.CredId   = jsonObj["c_creds_id"].toString();
        c.Username = jsonObj["c_username"].toString();
        c.Password = jsonObj["c_password"].toString();
        c.Realm    = jsonObj["c_realm"].toString();
        c.Type     = jsonObj["c_type"].toString();
        c.Tag      = jsonObj["c_tag"].toString();
        c.Storage  = jsonObj["c_storage"].toString();
        c.Host     = jsonObj["c_host"].toString();
        CredentialsDock->EditCredentialsItem(c);
        break;
    }

    case TYPE_CREDS_DELETE: {
        QStringList ids;
        for (const QJsonValue &val : jsonObj["c_creds_id"].toArray())
            if (val.isString()) ids.append(val.toString());
        CredentialsDock->RemoveCredentialsItem(ids);
        break;
    }

    case TYPE_CREDS_SET_TAG: {
        QStringList ids;
        for (const QJsonValue &val : jsonObj["c_creds_id"].toArray())
            ids.append(val.toString());
        CredentialsDock->CredsSetTag(ids, jsonObj["c_tag"].toString());
        break;
    }

    case TYPE_TARGETS_CREATE: {
        QList<TargetData> targetsList;
        for (const QJsonValue &val : jsonObj["t_targets"].toArray()) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();
            if (!obj.contains("t_target_id") || !obj["t_target_id"].isString()) continue;
            if (!obj.contains("t_computer")  || !obj["t_computer"].isString())  continue;
            if (!obj.contains("t_domain")    || !obj["t_domain"].isString())    continue;
            if (!obj.contains("t_address")   || !obj["t_address"].isString())   continue;
            if (!obj.contains("t_os")        || !obj["t_os"].isDouble())        continue;
            if (!obj.contains("t_os_desk")   || !obj["t_os_desk"].isString())   continue;
            if (!obj.contains("t_tag")       || !obj["t_tag"].isString())       continue;
            if (!obj.contains("t_info")      || !obj["t_info"].isString())      continue;
            if (!obj.contains("t_date")      || !obj["t_date"].isDouble())      continue;
            if (!obj.contains("t_alive")     || !obj["t_alive"].isBool())       continue;
            if (!obj.contains("t_agents")) continue;

            TargetData t;
            t.TargetId      = obj["t_target_id"].toString();
            t.Computer      = obj["t_computer"].toString();
            t.Domain        = obj["t_domain"].toString();
            t.Address       = obj["t_address"].toString();
            t.Os            = obj["t_os"].toInt();
            t.OsDesc        = obj["t_os_desk"].toString();
            t.Tag           = obj["t_tag"].toString();
            t.Info          = obj["t_info"].toString();
            t.DateTimestamp = static_cast<qint64>(obj["t_date"].toDouble());
            t.Date          = UnixTimestampGlobalToStringLocal(t.DateTimestamp);
            t.Alive         = obj["t_alive"].toBool();
            if (obj["t_agents"].isArray()) {
                for (const QJsonValue &aid : obj["t_agents"].toArray())
                    if (aid.isString()) t.Agents.append(aid.toString());
            }
            t.OsIcon = getTargetOsIcon(t.Os, !t.Agents.isEmpty(), t.Alive);
            targetsList.append(t);
        }
        TargetsDock->AddTargetsItems(targetsList);
        break;
    }

    case TYPE_TARGETS_EDIT: {
        TargetData t = {};
        t.TargetId      = jsonObj["t_target_id"].toString();
        t.Computer      = jsonObj["t_computer"].toString();
        t.Domain        = jsonObj["t_domain"].toString();
        t.Address       = jsonObj["t_address"].toString();
        t.Os            = jsonObj["t_os"].toDouble();
        t.OsDesc        = jsonObj["t_os_desk"].toString();
        t.Tag           = jsonObj["t_tag"].toString();
        t.Info          = jsonObj["t_info"].toString();
        t.DateTimestamp = static_cast<qint64>(jsonObj["t_date"].toDouble());
        t.Date          = UnixTimestampGlobalToStringLocal(t.DateTimestamp);
        t.Alive         = jsonObj["t_alive"].toBool();
        if (jsonObj["t_agents"].isArray()) {
            for (const QJsonValue &aid : jsonObj["t_agents"].toArray())
                if (aid.isString()) t.Agents.append(aid.toString());
        }
        t.OsIcon = getTargetOsIcon(t.Os, !t.Agents.isEmpty(), t.Alive);
        TargetsDock->EditTargetsItem(t);
        break;
    }

    case TYPE_TARGETS_DELETE: {
        QStringList ids;
        for (const QJsonValue &val : jsonObj["t_target_id"].toArray())
            if (val.isString()) ids.append(val.toString());
        TargetsDock->RemoveTargetsItem(ids);
        break;
    }

    case TYPE_TARGETS_SET_TAG: {
        QStringList ids;
        for (const QJsonValue &val : jsonObj["t_targets_id"].toArray())
            ids.append(val.toString());
        TargetsDock->TargetsSetTag(ids, jsonObj["t_tag"].toString());
        break;
    }

    case TYPE_TUNNEL_CREATE:
        TunnelsDock->AddTunnelItem(parseTunnelData(jsonObj));
        break;

    case TYPE_TUNNEL_EDIT:
        TunnelsDock->EditTunnelItem(jsonObj["p_tunnel_id"].toString(), jsonObj["p_info"].toString());
        break;

    case TYPE_TUNNEL_DELETE:
        TunnelsDock->RemoveTunnelItem(jsonObj["p_tunnel_id"].toString());
        break;

    case TYPE_BROWSER_DISKS: {
        QString agentId = jsonObj["b_agent_id"].toString();
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser())
                agent->GetFileBrowser()->SetDisksWin(
                    jsonObj["b_time"].toDouble(),
                    jsonObj["b_msg_type"].toDouble(),
                    jsonObj["b_message"].toString(),
                    jsonObj["b_data"].toString()
                );
        }
        break;
    }

    case TYPE_BROWSER_FILES: {
        QString agentId = jsonObj["b_agent_id"].toString();
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser())
                agent->GetFileBrowser()->AddFiles(
                    jsonObj["b_time"].toDouble(),
                    jsonObj["b_msg_type"].toDouble(),
                    jsonObj["b_message"].toString(),
                    jsonObj["b_path"].toString(),
                    jsonObj["b_data"].toString()
                );
        }
        break;
    }

    case TYPE_BROWSER_PROCESS: {
        QString agentId = jsonObj["b_agent_id"].toString();
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasProcessBrowser()) {
                agent->GetProcessBrowser()->SetStatus(
                    jsonObj["b_time"].toDouble(),
                    jsonObj["b_msg_type"].toDouble(),
                    jsonObj["b_message"].toString()
                );
                agent->GetProcessBrowser()->SetProcess(
                    jsonObj["b_msg_type"].toDouble(),
                    jsonObj["b_data"].toString()
                );
            }
        }
        break;
    }

    case TYPE_BROWSER_STATUS: {
        QString agentId = jsonObj["b_agent_id"].toString();
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser())
                agent->GetFileBrowser()->SetStatus(
                    jsonObj["b_time"].toDouble(),
                    jsonObj["b_msg_type"].toDouble(),
                    jsonObj["b_message"].toString()
                );
        }
        break;
    }

    case TYPE_PIVOT_CREATE: {
        PivotData pivotData = parsePivotData(jsonObj);
        if (AgentsMap.contains(pivotData.ParentAgentId) && AgentsMap.contains(pivotData.ChildAgentId)) {
            Agent* parentAgent = AgentsMap[pivotData.ParentAgentId];
            Agent* childAgent  = AgentsMap[pivotData.ChildAgentId];
            parentAgent->AddChild(pivotData);
            childAgent->SetParent(pivotData);
            Pivots[pivotData.PivotId] = pivotData;
            SessionsGraphDock->RelinkAgent(parentAgent, childAgent, pivotData.PivotName, this->synchronized);
            SessionsTableDock->UpdateAgentItem(childAgent->data, childAgent);
        }
        break;
    }

    case TYPE_PIVOT_DELETE: {
        QString pivotId = jsonObj["p_pivot_id"].toString();
        if (Pivots.contains(pivotId)) {
            PivotData pivotData = Pivots[pivotId];
            Pivots.remove(pivotId);
            if (AgentsMap.contains(pivotData.ParentAgentId) && AgentsMap.contains(pivotData.ChildAgentId)) {
                Agent* parentAgent = AgentsMap[pivotData.ParentAgentId];
                Agent* childAgent  = AgentsMap[pivotData.ChildAgentId];
                parentAgent->RemoveChild(pivotData);
                childAgent->UnsetParent(pivotData);
                SessionsGraphDock->UnlinkAgent(parentAgent, childAgent, this->synchronized);
                SessionsTableDock->UpdateAgentItem(childAgent->data, childAgent);
            }
        }
        break;
    }

    case SP_TYPE_EVENT:
        LogsDock->AddLogs(
            jsonObj["event_type"].toDouble(),
            jsonObj["date"].toDouble(),
            jsonObj["message"].toString()
        );
        break;

    default:
        break;
    }
}

void AdaptixWidget::setSyncUpdateUI(const bool enabled)
{
    if (AxConsoleDock)     AxConsoleDock->SetUpdatesEnabled(enabled);
    if (LogsDock)          LogsDock->SetUpdatesEnabled(enabled);
    if (ChatDock)          ChatDock->SetUpdatesEnabled(enabled);
    if (ListenersDock)     ListenersDock->SetUpdatesEnabled(enabled);
    if (SessionsTableDock) SessionsTableDock->SetUpdatesEnabled(enabled);
    if (TunnelsDock)       TunnelsDock->SetUpdatesEnabled(enabled);
    if (DownloadsDock)     DownloadsDock->SetUpdatesEnabled(enabled);
    if (ScreenshotsDock)   ScreenshotsDock->SetUpdatesEnabled(enabled);
    if (CredentialsDock)   CredentialsDock->SetUpdatesEnabled(enabled);
    if (TasksDock)         TasksDock->SetUpdatesEnabled(enabled);
    if (TargetsDock)       TargetsDock->SetUpdatesEnabled(enabled);

    for (const auto agent : AgentsMap.values())
        agent->Console->SetUpdatesEnabled(enabled);

    this->setEnabled(enabled);
}
