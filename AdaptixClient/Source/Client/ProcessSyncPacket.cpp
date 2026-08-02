#include <Agent/Agent.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/TunnelEndpoint.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/SessionWidgetIface.h>
#include <UI/Widgets/ListenersFeedWidget.h>
#include <UI/Widgets/PayloadsFeedWidget.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <UI/Widgets/ScreenshotsFeedWidget.h>
#include <UI/Widgets/TunnelsFeedWidget.h>
#include <UI/Widgets/CredentialWidgetIface.h>
#include <UI/Widgets/TargetWidgetIface.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Graph/GraphItem.h>
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
        data.Tags             = json["l_tags"].toString();
        data.Data             = json["l_data"].toString();
        return data;
    }

    TaskData parseTaskData(const QJsonObject &json) {
        TaskData data = {};
        data.TaskId      = parseI64(json, "a_task_id");
        data.TaskType    = json["a_task_type"].toDouble();
        data.AgentId     = parseI64(json, "a_id");
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

    TransferData parseTransferCreate(const QJsonObject &json) {
        TransferData data = {};
        data.TransferType  = static_cast<int>(json["t_type"].toDouble());
        data.FileId        = parseI64(json, "t_file_id");
        data.AgentId       = parseI64(json, "t_agent_id");
        data.AgentName     = json["t_agent_name"].toString();
        data.User          = json["t_user"].toString();
        data.Computer      = json["t_computer"].toString();
        data.Filename      = json["t_file"].toString();
        data.TotalSize     = static_cast<qint64>(json["t_size"].toDouble());
        data.DateTimestamp = static_cast<qint64>(json["t_date"].toDouble());
        data.Date          = UnixTimestampGlobalToStringLocal(data.DateTimestamp);
        data.Progress      = 0;
        data.State         = TRANSFER_STATE_RUNNING;
        data.Tag           = json["t_tag"].toString();
        data.Cancellable   = json.contains("t_cancellable") ? json["t_cancellable"].toBool() : true;
        data.Kind          = static_cast<int>(json["t_kind"].toDouble());
        data.ArtifactName  = json["t_artifact_name"].toString();
        data.ArtifactType  = json["t_artifact_type"].toString();
        return data;
    }

    TransferData parseTransferActual(const QJsonObject &json) {
        TransferData data = {};
        data.TransferType  = static_cast<int>(json["t_type"].toDouble());
        data.FileId        = parseI64(json, "t_file_id");
        data.AgentId       = parseI64(json, "t_agent_id");
        data.AgentName     = json["t_agent_name"].toString();
        data.User          = json["t_user"].toString();
        data.Computer      = json["t_computer"].toString();
        data.Filename      = json["t_file"].toString();
        data.TotalSize     = static_cast<qint64>(json["t_size"].toDouble());
        data.DateTimestamp = static_cast<qint64>(json["t_date"].toDouble());
        data.Date          = UnixTimestampGlobalToStringLocal(data.DateTimestamp);
        data.Progress      = static_cast<qint64>(json["t_progress"].toDouble());
        data.State         = static_cast<int>(json["t_state"].toDouble());
        data.Tag           = json["t_tag"].toString();
        data.Cancellable   = json.contains("t_cancellable") ? json["t_cancellable"].toBool() : true;
        data.Kind          = static_cast<int>(json["t_kind"].toDouble());
        data.ArtifactName  = json["t_artifact_name"].toString();
        data.ArtifactType  = json["t_artifact_type"].toString();
        return data;
    }

    ScreenData parseScreenData(const QJsonObject &json) {
        ScreenData data = {};
        data.ScreenId      = parseI64(json, "s_screen_id");
        data.AgentId       = parseI64(json, "s_agent_id");
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
        data.TunnelId  = parseI64(json, "p_tunnel_id");
        data.AgentId   = parseI64(json, "p_agent_id");
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
        data.DateTimestamp = static_cast<qint64>(json["p_date"].toDouble());
        data.BytesSent = static_cast<qint64>(json["p_bytes_sent"].toDouble());
        data.BytesRecv = static_cast<qint64>(json["p_bytes_recv"].toDouble());
        data.Active = json.contains(QStringLiteral("p_active")) ? json["p_active"].toBool() : true;
        return data;
    }

    PivotData parsePivotData(const QJsonObject &json) {
        PivotData data = {};
        data.PivotId       = json["p_pivot_id"].toString();
        data.PivotName     = json["p_pivot_name"].toString();
        data.ParentAgentId = parseI64(json, "p_parent_agent_id");
        data.ChildAgentId  = parseI64(json, "p_child_agent_id");
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

    case TYPE_LOG_BATCH:
        return checkField("items", isArr);

    case TYPE_REG_SERVICE:
        return checkField("service", isStr) &&
               checkField("ax", isStr);

    case TYPE_PLUGIN_SERVICE_DATA:
        return checkField("service", isStr) &&
               checkField("data", isStr);

    case TYPE_PLUGIN_AGENT_DATA:
        return checkField("agent_id", isNum) &&
               checkField("data", isStr);

    case TYPE_PLUGIN_LISTENER_DATA:
        return checkField("listener", isStr) &&
               checkField("data", isStr);

    case TYPE_REG_LISTENER:
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

    case TYPE_REG_AGENT:
        return checkField("agent", isStr) &&
               checkField("ax", isStr) &&
               checkField("listeners", isArr) &&
               checkField("multi_listeners", isBl) &&
               checkField("groups", isArr);

    case TYPE_AGENT_NEW:
        return checkField("a_id", isNum) &&
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
        return checkField("a_id", isNum);

    case TYPE_AGENT_REMOVE:
        return checkField("a_id", isNum);

    case TYPE_AGENT_TASK_SYNC:
        return checkField("a_id", isNum) &&
               checkField("a_task_id", isNum) &&
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
        return checkField("a_id", isNum) &&
               checkField("a_task_id", isNum) &&
               checkField("a_task_type", isNum) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_TASK_SEND:
        return checkField("a_task_id", isArr);

    case TYPE_AGENT_TASK_REMOVE:
        return checkField("a_task_id", isNum);

    case TYPE_AGENT_TASK_HOOK:
        return checkField("a_id", isNum) &&
               checkField("a_task_id", isNum) &&
               checkField("a_hook_id", isStr) &&
               checkField("a_job_index", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_CONSOLE_OUT:
        return checkField("time", isNum) &&
               checkField("a_id", isNum) &&
               checkField("a_text", isStr) &&
               checkField("a_message", isStr) &&
               checkField("a_msg_type", isNum);

    case TYPE_AGENT_CONSOLE_LOCAL:
        return checkField("time", isNum) &&
                checkField("a_id", isNum) &&
                checkField("a_cmdline", isStr) &&
                checkField("a_text", isStr) &&
                checkField("a_message", isStr);

    case TYPE_AGENT_CONSOLE_ERROR:
        return checkField("a_id", isNum) &&
               checkField("a_cmdline", isStr) &&
               checkField("a_message", isStr) &&
               checkField("ax_hook_id", isStr) &&
               checkField("ax_handler_id", isStr);

    case TYPE_AGENT_CONSOLE_TASK_SYNC:
        return checkField("a_id", isNum) &&
               checkField("a_task_id", isNum) &&
               checkField("a_start_time", isNum) &&
               checkField("a_cmdline", isStr) &&
               checkField("a_client", isStr) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_AGENT_CONSOLE_TASK_UPD:
        return checkField("a_id", isNum) &&
               checkField("a_task_id", isNum) &&
               checkField("a_finish_time", isNum) &&
               checkField("a_msg_type", isNum) &&
               checkField("a_message", isStr) &&
               checkField("a_text", isStr) &&
               checkField("a_completed", isBl);

    case TYPE_CHAT_MESSAGE:
        return checkField("c_id", isNum) &&
               checkField("c_username", isStr) &&
               checkField("c_message", isStr) &&
               checkField("c_date", isNum) &&
               checkField("c_edited", isBl) &&
               checkField("c_deleted", isBl) &&
               checkField("c_deleted_date", isNum) &&
               checkField("c_reactions", isStr) &&
               checkField("c_reply_to_id", isNum) &&
               checkField("c_reply_to_name", isStr);

    case TYPE_CHAT_EDIT:
        return checkField("c_id", isNum) &&
               checkField("c_message", isStr);

    case TYPE_CHAT_DELETE:
        return checkField("c_id", isNum);

    case TYPE_CHAT_REACTION:
        return checkField("c_id", isNum) &&
               checkField("c_reactions", isStr);

    case TYPE_CHAT_TODO:
        return checkField("c_content", isStr) &&
               checkField("c_updated_by", isStr) &&
               checkField("c_updated_at", isNum);

        case TYPE_TRANSFER_CREATE:
            return checkField("t_type", isNum) &&
                checkField("t_agent_id", isNum) &&
                checkField("t_file_id", isNum) &&
                checkField("t_agent_name", isStr) &&
                checkField("t_user", isStr) &&
                checkField("t_computer", isStr) &&
                checkField("t_file", isStr) &&
                checkField("t_size", isNum) &&
                checkField("t_date", isNum);

        case TYPE_TRANSFER_UPDATE:
            return checkField("t_type", isNum) &&
                checkField("t_file_id", isNum) &&
                checkField("t_progress", isNum) &&
                checkField("t_state", isNum);

        case TYPE_TRANSFER_DELETE:
            return checkField("t_type", isNum) &&
                checkField("t_files_id", isArr);

        case TYPE_TRANSFER_SET_TAG:
            return checkField("t_type", isNum) &&
                checkField("t_files_id", isArr) &&
                checkField("t_tag", isStr);

        case TYPE_TRANSFER_ACTUAL:
            return checkField("t_type", isNum) &&
                checkField("t_file_id", isNum) &&
                checkField("t_agent_id", isNum) &&
                checkField("t_agent_name", isStr) &&
                checkField("t_user", isStr) &&
                checkField("t_computer", isStr) &&
                checkField("t_file", isStr) &&
                checkField("t_size", isNum) &&
                checkField("t_date", isNum) &&
                checkField("t_progress", isNum) &&
                checkField("t_state", isNum);

    case TYPE_TUNNEL_CREATE:
        return checkField("p_tunnel_id", isNum) &&
               checkField("p_agent_id", isNum) &&
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
        return checkField("p_tunnel_id", isNum) &&
               checkField("p_info", isStr);

    case TYPE_TUNNEL_DELETE:
        return checkField("p_tunnel_id", isNum);

    case TYPE_SCREEN_CREATE:
        return checkField("s_screen_id", isNum) &&
               checkField("s_agent_id", isNum) &&
               checkField("s_user", isStr) &&
               checkField("s_computer", isStr) &&
               checkField("s_note", isStr) &&
               checkField("s_date", isNum) &&
               checkField("s_content", isStr);

    case TYPE_SCREEN_UPDATE:
        return checkField("s_screen_id", isNum) &&
               checkField("s_note", isStr);

    case TYPE_SCREEN_DELETE:
        return checkField("s_screen_id", isNum);

    case TYPE_CREDS_CREATE:
        return checkField("c_creds", isArr);

    case TYPE_CREDS_EDIT:
        return checkField("c_creds_id", isNum) &&
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
        return checkField("t_target_id", isNum) &&
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
        return checkField("b_agent_id", isNum) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_data", isStr);

    case TYPE_BROWSER_FILES:
        return checkField("b_agent_id", isNum) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_path", isStr) &&
               checkField("b_data", isStr);

    case TYPE_BROWSER_STATUS:
        return checkField("b_agent_id", isNum) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr);

    case TYPE_BROWSER_PROCESS:
        return checkField("b_agent_id", isNum) &&
               checkField("b_time", isNum) &&
               checkField("b_msg_type", isNum) &&
               checkField("b_message", isStr) &&
               checkField("b_data", isStr);

    case TYPE_PIVOT_CREATE:
        return checkField("p_pivot_id", isStr) &&
               checkField("p_pivot_name", isStr) &&
               checkField("p_parent_agent_id", isNum) &&
               checkField("p_child_agent_id", isNum);

    case TYPE_PIVOT_DELETE:
        return checkField("p_pivot_id", isStr);

    case TYPE_GROUP_CREATE:
        return checkField("g_group_id", isNum) &&
               checkField("g_parent_group_id", isNum) &&
               checkField("g_group_name", isStr) &&
               checkField("g_scope", isStr) &&
               checkField("g_members", isArr);

    case TYPE_GROUP_RENAME:
        return checkField("g_group_id", isNum) &&
               checkField("g_group_name", isStr);

    case TYPE_GROUP_DELETE:
        return checkField("g_group_id", isNum);

    case TYPE_GROUP_MEMBERS:
        return checkField("g_group_id", isNum) &&
               checkField("g_add", isArr) &&
               checkField("g_remove", isArr);

    case TYPE_GROUP_REPARENT:
        return checkField("g_group_id", isNum) &&
               checkField("g_new_parent_id", isNum);

    case TYPE_AXSCRIPT_COMMANDS:
        return checkField("name", isStr) &&
               checkField("content", isStr) &&
               checkField("groups", isArr);

    case TYPE_AXSCRIPT_LIST:
        return checkField("items", isArr);

    case TYPE_EVENT_HANDLERS:
        return checkField("items", isArr);

    case TYPE_PAYLOAD_CREATE:
    case TYPE_PAYLOAD_EDIT:
        return checkField("p_id", isNum) &&
               checkField("p_name", isStr) &&
               checkField("p_type", isStr) &&
               checkField("p_artifact", isStr) &&
               checkField("p_arch", isStr) &&
               checkField("p_size", isNum) &&
               checkField("p_sha1", isStr) &&
               checkField("p_sha256", isStr) &&
               checkField("p_md5", isStr) &&
               checkField("p_creator", isStr) &&
               checkField("p_date", isNum) &&
               checkField("p_filename", isStr);

    case TYPE_PAYLOAD_UPDATE:
        return checkField("p_ids", isArr) &&
               checkField("p_hidden", isBl);

    case TYPE_PAYLOAD_DELETE:
        return checkField("p_ids", isArr);

    default:
        qWarning() << "[SyncPacket] Unknown packet type:" << spType;
        return false;
    }
}

void AdaptixWidget::replayDeferredTaskPackets(qint64 taskId)
{
    auto deferred = deferredTaskPackets.values(taskId);
    deferredTaskPackets.remove(taskId);
    for (const auto &pkt : deferred)
        processSyncPacket(pkt);
}

void AdaptixWidget::replayDeferredTransferPackets(qint64 fileId)
{
    auto deferred = deferredTransferPackets.values(fileId);
    deferredTransferPackets.remove(fileId);
    for (const auto &pkt : deferred)
        processSyncPacket(pkt);
}

void AdaptixWidget::processSyncPacket(QJsonObject jsonObj)
{
    int spType = jsonObj["type"].toDouble();

    if (spType == TYPE_SYNC_BATCH || spType == TYPE_SYNC_CATEGORY_BATCH) {
        if (this->sync && dialogSyncPacket) {
            dialogSyncPacket->receivedLogs++;
            dialogSyncPacket->upgrade();
        }

        QJsonArray packetsArray = jsonObj["packets"].toArray();

        QJsonObject marker;
        marker.insert("__ax_batch_marker", true);
        marker.insert("__ax_batch_size", packetsArray.size());
        enqueueSyncPacket(marker);

        for (const QJsonValue &packetValue : packetsArray) {
            if (packetValue.isObject())
                enqueueSyncPacket(packetValue.toObject());
        }
        return;
    }

    switch (spType) {

    case TYPE_SYNC_START: {
        int count = jsonObj["count"].toDouble();
        QJsonArray interfaces = jsonObj["interfaces"].toArray();
        this->addresses.clear();
        for (const QJsonValue &addrValue : interfaces) {
            this->addresses.append(addrValue.toString());
        }

        if (LogsDock)
            LogsDock->ResetServerLogs();

        if (count <= 0) {
            this->sync = true;
            this->syncFinishReceived = false;
            this->syncTotalBatches = 0;
            this->syncProcessingBatchIndex = 0;
            this->syncProcessingBatchTotal = 0;
            this->syncProcessingBatchProcessed = 0;
            this->setSyncUpdateUI(false);
            if (dialogSyncPacket)
                dialogSyncPacket->init(count);
            break;
        }

        this->sync = true;
        this->syncFinishReceived = false;
        this->syncTotalBatches = count;
        this->syncProcessingBatchIndex = 0;
        this->syncProcessingBatchTotal = 0;
        this->syncProcessingBatchProcessed = 0;
        this->setSyncUpdateUI(false);
        if (dialogSyncPacket && dialogSyncPacket->splashScreen) {
            dialogSyncPacket->splashScreen->show();
            dialogSyncPacket->splashScreen->raise();
            dialogSyncPacket->splashScreen->activateWindow();
        }
        if (dialogSyncPacket)
            dialogSyncPacket->init(count);
        break;
    }

    case TYPE_SYNC_FINISH:
        if (!this->sync)
            break;

        this->syncFinishReceived = true;
        finalizeSyncIfReady();
        if (LogsDock)
            LogsDock->ReloadServerLogs();

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

    case TYPE_REG_SERVICE:
        this->RegisterServiceConfig( jsonObj["service"].toString(), jsonObj["ax"].toString() );
        break;

    case TYPE_PLUGIN_SERVICE_DATA:
        ScriptManager->PluginServiceDataHandler( jsonObj["service"].toString(), jsonObj["data"].toString() );
        break;

    case TYPE_PLUGIN_AGENT_DATA:
        ScriptManager->PluginAgentDataHandler(parseI64(jsonObj, "agent_id"), jsonObj["agent_type"].toString(), jsonObj["data"].toString() );
        break;

    case TYPE_PLUGIN_LISTENER_DATA:
        ScriptManager->PluginListenerDataHandler(jsonObj["listener"].toString(), jsonObj["listener_type"].toString(), jsonObj["data"].toString() );
        break;

    case TYPE_REG_LISTENER:
        this->RegisterListenerConfig( jsonObj["l_name"].toString(), jsonObj["l_protocol"].toString(), jsonObj["l_type"].toString(), jsonObj["ax"].toString() );
        break;

    case TYPE_AGENT_NEW: {

        qint64 agentId = parseI64(jsonObj, "a_id");
        {
            QReadLocker locker(&AgentsMapLock);
            if (AgentsMap.contains(agentId))
                break;
        }
        QWidget* previousFocus = QApplication::focusWidget();

        Agent* newAgent = new Agent(jsonObj, this);
        {
            QWriteLocker locker(&AgentsMapLock);
            AgentsMap[agentId] = newAgent;
        }
        SessionsTableDock->AddAgentItem(newAgent);
        SessionsGraphDock->AddAgent(newAgent, this->synchronized);
        if (synchronized) {
            TasksDock->UpdateFilterComboBoxes();
            CredentialsDock->UpdateFilterComboBoxes();
            DownloadsDock->UpdateFilterComboBoxes();
            Q_EMIT eventNewAgent(newAgent->data.Id);
        }

        if (previousFocus)
            previousFocus->setFocus();

        break;
    }

    case TYPE_AGENT_UPDATE: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        QWriteLocker locker(&AgentsMapLock);
        Agent* agent = AgentsMap.value(agentId, nullptr);
        if (agent) {
            auto oldData = agent->data;
            agent->Update(jsonObj);
            locker.unlock();
            SessionsTableDock->UpdateAgentItem(oldData, agent);

            const bool userHostChanged =
                oldData.Username     != agent->data.Username ||
                oldData.Computer     != agent->data.Computer ||
                oldData.Domain       != agent->data.Domain   ||
                oldData.Impersonated != agent->data.Impersonated ||
                oldData.Elevated     != agent->data.Elevated;
            const bool processChanged =
                oldData.Process != agent->data.Process ||
                oldData.Pid     != agent->data.Pid     ||
                oldData.Arch    != agent->data.Arch;
            const bool markChanged = oldData.Mark != agent->data.Mark;

            if (userHostChanged || processChanged) {
                if (agent->Console)    agent->Console->UpdateInfoLabel();
                if (agent->graphItem)  agent->graphItem->UpdateNote();
            }
            if (markChanged)
                Q_EMIT agentTickUpdated(agentId);
        }
        break;
    }

    case TYPE_AGENT_TICK: {
        QJsonArray agentIDs = jsonObj["a_id"].toArray();
        QWriteLocker locker(&AgentsMapLock);
        for (const QJsonValue &idValue : agentIDs) {
            Agent* agent = AgentsMap.value(parseI64(idValue), nullptr);
            if (agent) {
                agent->data.LastTick = QDateTime::currentSecsSinceEpoch();
                if (agent->data.Mark != "Terminated")
                    agent->MarkItem("");
            }
        }
        break;
    }

    case TYPE_AGENT_REMOVE: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        Agent* agentToRemove = nullptr;
        {
            QWriteLocker locker(&AgentsMapLock);
            agentToRemove = AgentsMap.take(agentId);
        }
        if (agentToRemove) {
            SessionsGraphDock->RemoveAgent(agentToRemove, this->synchronized);
            if (SessionsTableDock)
                SessionsTableDock->RemoveAgentItem(agentId);
            if (TasksDock)
                TasksDock->RemoveAgentTasksItem(agentId);
            for (auto it = GraphTunnelMarks.begin(); it != GraphTunnelMarks.end(); ) {
                if (it.value().first == agentId)
                    it = GraphTunnelMarks.erase(it);
                else
                    ++it;
            }
            delete agentToRemove;
        }
        break;
    }

    case TYPE_REG_AGENT: {
        QStringList listeners;
        for (const QJsonValue &listener : jsonObj["listeners"].toArray())
            listeners.append(listener.toString());
        this->RegisterAgentConfig(jsonObj["agent"].toString(), jsonObj["ax"].toString(), listeners, jsonObj["multi_listeners"].toBool(), jsonObj["groups"].toArray());
        break;
    }

    case TYPE_AGENT_TASK_SYNC: {
        TaskData td = parseTaskData(jsonObj);
        {
            QWriteLocker locker(&TasksMapLock);
            TasksMap[td.TaskId] = td;
        }
        TasksDock->AddTaskItem(td);
        replayDeferredTaskPackets(td.TaskId);
        break;
    }

    case TYPE_AGENT_TASK_UPDATE: {
        qint64 taskId = parseI64(jsonObj, "a_task_id");
        TaskData taskCopy;
        bool found = false;
        QString handlerId;
        {
            QWriteLocker locker(&TasksMapLock);
            if (!TasksMap.contains(taskId)) {
                locker.unlock();
                deferredTaskPackets.insert(taskId, jsonObj);
                break;
            }
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
            else if (jsonObj.contains("a_message") && !jsonObj["a_message"].toString().isEmpty()) {
                QString msg = jsonObj["a_message"].toString();
                if (!msg.isEmpty())
                    task->Message = msg;
            }
            task->Output += jsonObj["a_text"].toString();
            taskCopy = *task;
            found = true;
            if (task->Completed && jsonObj.contains("a_handler_id") && jsonObj["a_handler_id"].isString())
                handlerId = jsonObj["a_handler_id"].toString();
        }
        if (found)
            TasksDock->UpdateTaskItem(taskId, taskCopy);

        if (found && taskCopy.Completed && !handlerId.isEmpty()) {
            QReadLocker hLocker(&PostHandlersLock);
            if (PostHandlersJS.contains(handlerId)) {
                hLocker.unlock();
                this->PostHandlerProcess(handlerId, taskCopy);
            }
        }
        break;
    }

    case TYPE_AGENT_TASK_SEND: {
        QList<TaskData> toUpdate;
        {
            QWriteLocker locker(&TasksMapLock);
            for (const QJsonValue &idValue : jsonObj["a_task_id"].toArray()) {
                qint64 id = parseI64(idValue);
                if (TasksMap.contains(id)) {
                    TasksMap[id].Status = "Running";
                    toUpdate.append(TasksMap[id]);
                } else {
                    deferredTaskPackets.insert(id, jsonObj);
                }
            }
        }
        for (const TaskData& t : toUpdate)
            TasksDock->UpdateTaskItem(t.TaskId, t);
        break;
    }

    case TYPE_AGENT_TASK_REMOVE:
        TasksDock->RemoveTaskItem(parseI64(jsonObj, "a_task_id"));
        break;

    case TYPE_AGENT_TASK_HOOK:
        this->PostHookProcess(jsonObj);
        break;

    case TYPE_AGENT_CONSOLE_OUT: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        QReadLocker locker(&AgentsMapLock);
        if (AgentsMap.contains(agentId) && AgentsMap[agentId]->Console) {
            AgentsMap[agentId]->Console->ConsoleOutputMessage( static_cast<qint64>(jsonObj["time"].toDouble()), "", jsonObj["a_msg_type"].toDouble(), jsonObj["a_message"].toString(), jsonObj["a_text"].toString(), false );
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_LOCAL: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        QReadLocker locker(&AgentsMapLock);
        if (AgentsMap.contains(agentId) && AgentsMap[agentId]->Console) {
            const qint64 t = static_cast<qint64>(jsonObj["time"].toDouble());
            AgentsMap[agentId]->Console->ConsoleOutputPrompt(t, "", "", jsonObj["a_cmdline"].toString());
            AgentsMap[agentId]->Console->ConsoleOutputMessage(t, "", CONSOLE_OUT_LOCAL_INFO, jsonObj["a_message"].toString(), jsonObj["a_text"].toString(), false);
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_ERROR: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        {
            QWriteLocker hkLocker(&PostHooksLock);
            this->PostHooksJS.remove(jsonObj["ax_hook_id"].toString());
        }
        {
            QWriteLocker hdLocker(&PostHandlersLock);
            this->PostHandlersJS.remove(jsonObj["ax_handler_id"].toString());
        }
        QReadLocker locker(&AgentsMapLock);
        if (AgentsMap.contains(agentId) && AgentsMap[agentId]->Console) {
            const qint64 t = static_cast<qint64>(jsonObj["time"].toDouble());
            AgentsMap[agentId]->Console->ConsoleOutputPrompt(t, "", "", jsonObj["a_cmdline"].toString());
            AgentsMap[agentId]->Console->ConsoleOutputMessage(t, "", CONSOLE_OUT_LOCAL_ERROR, jsonObj["a_message"].toString(), "", true);
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_TASK_SYNC: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        QReadLocker locker(&AgentsMapLock);
        if (AgentsMap.contains(agentId) && AgentsMap[agentId]->Console) {
            qint64 startTime = jsonObj["a_start_time"].toDouble();
            qint64 finishTime = jsonObj["a_finish_time"].toDouble();
            bool completed = jsonObj["a_completed"].toBool();
            QString taskIdStr = QString::number(parseI64(jsonObj, "a_task_id"));
            AgentsMap[agentId]->Console->ConsoleOutputPrompt( startTime, taskIdStr, jsonObj["a_client"].toString(), jsonObj["a_cmdline"].toString() );
            AgentsMap[agentId]->Console->ConsoleOutputMessage( completed ? finishTime : startTime, taskIdStr, jsonObj["a_msg_type"].toDouble(), jsonObj["a_message"].toString(), jsonObj["a_text"].toString(), completed );
        }
        break;
    }

    case TYPE_AGENT_CONSOLE_TASK_UPD: {
        qint64 agentId = parseI64(jsonObj, "a_id");
        QReadLocker locker(&AgentsMapLock);
        if (AgentsMap.contains(agentId) && AgentsMap[agentId]->Console)
            AgentsMap[agentId]->Console->ConsoleOutputMessage( jsonObj["a_finish_time"].toDouble(), QString::number(parseI64(jsonObj, "a_task_id")), jsonObj["a_msg_type"].toDouble(), jsonObj["a_message"].toString(), jsonObj["a_text"].toString(), jsonObj["a_completed"].toBool() );
        break;
    }

    case TYPE_CHAT_MESSAGE:
        ChatDock->AddChatMessage(
            (qint64)jsonObj["c_id"].toDouble(),
            jsonObj["c_username"].toString(),
            jsonObj["c_message"].toString(),
            (qint64)jsonObj["c_date"].toDouble(),
            jsonObj["c_edited"].toBool(),
            jsonObj["c_deleted"].toBool(),
            jsonObj["c_reactions"].toString(),
            (qint64)jsonObj["c_reply_to_id"].toDouble(),
            jsonObj["c_reply_to_name"].toString()
        );
        break;

    case TYPE_CHAT_EDIT:
        ChatDock->EditChatMessage(
            (qint64)jsonObj["c_id"].toDouble(),
            jsonObj["c_message"].toString()
        );
        break;

    case TYPE_CHAT_DELETE:
        ChatDock->DeleteChatMessage((qint64)jsonObj["c_id"].toDouble());
        break;

    case TYPE_CHAT_REACTION:
        ChatDock->UpdateReactions(
            (qint64)jsonObj["c_id"].toDouble(),
            jsonObj["c_reactions"].toString()
        );
        break;

    case TYPE_CHAT_TODO:
        ChatDock->SetTodo(
            jsonObj["c_content"].toString(),
            jsonObj["c_updated_by"].toString(),
            (qint64)jsonObj["c_updated_at"].toDouble()
        );
        break;

    case TYPE_TRANSFER_CREATE: {
        TransferData td = parseTransferCreate(jsonObj);
        DownloadsDock->AddTransferItem(td);
        replayDeferredTransferPackets(td.FileId);
        UpdateDownloadsBadge();
        break;
    }

    case TYPE_TRANSFER_UPDATE: {
        qint64 fileId = parseI64(jsonObj, "t_file_id");
        int transferType = static_cast<int>(jsonObj["t_type"].toDouble());
        bool isDownload = (transferType == TRANSFER_DOWNLOAD);
        auto &mapRef  = isDownload ? Downloads : Uploads;
        auto &lockRef = isDownload ? DownloadsLock : UploadsLock;
        bool inMap = false;
        {
            QReadLocker locker(&lockRef);
            inMap = mapRef.contains(fileId);
        }
        if (!inMap && !DownloadsDock->modelContainsTransfer(transferType, fileId)) {
            deferredTransferPackets.insert(fileId, jsonObj);
            break;
        }
        DownloadsDock->EditTransferItem(transferType, fileId, parseI64(jsonObj, "t_progress"), static_cast<int>(jsonObj["t_state"].toDouble()));
        UpdateDownloadsBadge();
        break;
    }

    case TYPE_TRANSFER_DELETE: {
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["t_files_id"].toArray()) {
            ids.append(parseI64(val));
        }
        DownloadsDock->RemoveTransferItem(static_cast<int>(jsonObj["t_type"].toDouble()), ids);
        UpdateDownloadsBadge();
        break;
    }

    case TYPE_TRANSFER_ACTUAL: {
        TransferData td = parseTransferActual(jsonObj);
        DownloadsDock->AddTransferItem(td);
        replayDeferredTransferPackets(td.FileId);
        UpdateDownloadsBadge();
        break;
    }

    case TYPE_TRANSFER_SET_TAG: {
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["t_files_id"].toArray()) {
            ids.append(parseI64(val));
        }
        DownloadsDock->SetTransferTag(static_cast<int>(jsonObj["t_type"].toDouble()), ids, jsonObj["t_tag"].toString());
        break;
    }

    case TYPE_SCREEN_CREATE:
        ScreenshotsDock->AddScreenshotItem(parseScreenData(jsonObj));
        break;

    case TYPE_SCREEN_UPDATE:
        ScreenshotsDock->EditScreenshotItem( parseI64(jsonObj, "s_screen_id"), jsonObj["s_note"].toString() );
        break;

    case TYPE_SCREEN_DELETE:
        ScreenshotsDock->RemoveScreenshotItem(parseI64(jsonObj, "s_screen_id"));
        break;

    case TYPE_CREDS_CREATE: {
        QList<CredentialData> credList;
        for (const QJsonValue &val : jsonObj["c_creds"].toArray()) {
            if (!val.isObject())
                continue;
            QJsonObject obj = val.toObject();
            if (!obj.contains("c_creds_id") || !obj["c_creds_id"].isDouble()) continue;
            if (!obj.contains("c_username") || !obj["c_username"].isString()) continue;
            if (!obj.contains("c_password") || !obj["c_password"].isString()) continue;
            if (!obj.contains("c_realm")    || !obj["c_realm"].isString())    continue;
            if (!obj.contains("c_type")     || !obj["c_type"].isString())     continue;
            if (!obj.contains("c_tag")      || !obj["c_tag"].isString())      continue;
            if (!obj.contains("c_date")     || !obj["c_date"].isDouble())     continue;
            if (!obj.contains("c_storage")  || !obj["c_storage"].isString())  continue;
            if (!obj.contains("c_agent_id") || !obj["c_agent_id"].isDouble()) continue;
            if (!obj.contains("c_host")     || !obj["c_host"].isString())     continue;

            CredentialData c;
            c.CredId        = parseI64(obj, "c_creds_id");
            c.Username      = obj["c_username"].toString();
            c.Password      = obj["c_password"].toString();
            c.Realm         = obj["c_realm"].toString();
            c.Type          = obj["c_type"].toString();
            c.Tag           = obj["c_tag"].toString();
            c.DateTimestamp = static_cast<qint64>(obj["c_date"].toDouble());
            c.Date          = UnixTimestampGlobalToStringLocal(c.DateTimestamp);
            c.Storage       = obj["c_storage"].toString();
            c.AgentId       = parseI64(obj, "c_agent_id");
            c.Host          = obj["c_host"].toString();
            credList.append(c);
        }
        CredentialsDock->AddCredentialsItems(credList);
        break;
    }

    case TYPE_CREDS_EDIT: {
        CredentialData c = {};
        c.CredId   = jsonObj["c_creds_id"].toDouble();
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
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["c_creds_id"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        CredentialsDock->RemoveCredentialsItem(ids);
        break;
    }

    case TYPE_CREDS_SET_TAG: {
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["c_creds_id"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        CredentialsDock->CredsSetTag(ids, jsonObj["c_tag"].toString());
        break;
    }

    case TYPE_TARGETS_CREATE: {
        QList<TargetData> targetsList;
        for (const QJsonValue &val : jsonObj["t_targets"].toArray()) {
            if (!val.isObject())
                continue;
            QJsonObject obj = val.toObject();
            if (!obj.contains("t_target_id") || !obj["t_target_id"].isDouble()) continue;
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
            t.TargetId      = static_cast<qint64>(obj["t_target_id"].toDouble());
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
                for (const QJsonValue &aid : obj["t_agents"].toArray()) {
                    t.Agents.append(parseI64(aid));
                }
            }
            t.OsIcon = getTargetOsIcon(t.Os, !t.Agents.isEmpty(), t.Alive);
            targetsList.append(t);
        }
        TargetsDock->AddTargetsItems(targetsList);
        break;
    }

    case TYPE_TARGETS_EDIT: {
        TargetData t = {};
        t.TargetId      = static_cast<qint64>(jsonObj["t_target_id"].toDouble());
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
            for (const QJsonValue &aid : jsonObj["t_agents"].toArray()) {
                t.Agents.append(parseI64(aid));
            }
        }
        t.OsIcon = getTargetOsIcon(t.Os, !t.Agents.isEmpty(), t.Alive);
        TargetsDock->EditTargetsItem(t);
        break;
    }

    case TYPE_TARGETS_DELETE: {
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["t_target_id"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        TargetsDock->RemoveTargetsItem(ids);
        break;
    }

    case TYPE_TARGETS_SET_TAG: {
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["t_targets_id"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        TargetsDock->TargetsSetTag(ids, jsonObj["t_tag"].toString());
        break;
    }

    case TYPE_PAYLOAD_CREATE:
    case TYPE_PAYLOAD_EDIT: {
        if (!PayloadsDock)
            break;
        PayloadData p;
        p.PayloadId = parseI64(jsonObj, "p_id");
        p.Name = jsonObj["p_name"].toString();
        p.AgentType = jsonObj["p_type"].toString();
        p.Artifact = jsonObj["p_artifact"].toString();
        p.Arch = jsonObj["p_arch"].toString();
        if (jsonObj["p_listeners"].isArray()) {
            for (const QJsonValue &lv : jsonObj["p_listeners"].toArray())
                p.Listeners << lv.toString();
        }
        p.Size = parseI64(jsonObj, "p_size");
        p.Sha1 = jsonObj["p_sha1"].toString();
        p.Sha256 = jsonObj["p_sha256"].toString();
        p.Md5 = jsonObj["p_md5"].toString();
        p.Creator = jsonObj["p_creator"].toString();
        p.Created = parseI64(jsonObj, "p_date");
        p.Hidden = jsonObj["p_hidden"].toBool();
        p.Filename = jsonObj["p_filename"].toString();
        p.BuildId = jsonObj["p_build_id"].toString();
        p.Watermark = jsonObj["p_watermark"].toString();
        p.Description = jsonObj["p_notes"].toString();
        p.Uid = jsonObj["p_uid"].toString();
        p.Color = jsonObj["p_color"].toString();
        p.Missing = jsonObj["p_missing"].toBool();
        if (spType == TYPE_PAYLOAD_EDIT)
            PayloadsDock->UpdatePayloadItem(p);
        else
            PayloadsDock->AddPayloadItem(p);
        break;
    }

    case TYPE_PAYLOAD_UPDATE: {
        if (!PayloadsDock)
            break;
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["p_ids"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        PayloadsDock->UpdatePayloadHidden(ids, jsonObj["p_hidden"].toBool());
        break;
    }

    case TYPE_PAYLOAD_DELETE: {
        if (!PayloadsDock)
            break;
        QList<qint64> ids;
        for (const QJsonValue &val : jsonObj["p_ids"].toArray()) {
            if (val.isDouble())
                ids.append(static_cast<qint64>(val.toDouble()));
        }
        PayloadsDock->RemovePayloadItems(ids);
        break;
    }

    case TYPE_TUNNEL_CREATE: {
        TunnelData tunnelData = parseTunnelData(jsonObj);
        TunnelsDock->AddTunnelItem(tunnelData);

        if (!tunnelData.Active && ClientTunnels.contains(tunnelData.TunnelId)) {
            auto* endpoint = ClientTunnels.value(tunnelData.TunnelId, nullptr);
            if (endpoint)
                endpoint->Stop();
        }

        if (!GraphTunnelMarks.contains(tunnelData.TunnelId)) {
            const TunnelMarkType mark = tunnelData.Client.isEmpty() ? TunnelMarkServer : TunnelMarkClient;
            GraphTunnelMarks.insert(tunnelData.TunnelId, { tunnelData.AgentId, static_cast<int>(mark) });
            QReadLocker locker(&AgentsMapLock);
            if (AgentsMap.contains(tunnelData.AgentId)) {
                Agent* agent = AgentsMap.value(tunnelData.AgentId, nullptr);
                if (agent && agent->graphItem)
                    agent->graphItem->AddTunnel(mark);
            }
        }
        break;
    }

    case TYPE_TUNNEL_EDIT: {
        const qint64 tid = parseI64(jsonObj, "p_tunnel_id");
        TunnelsDock->EditTunnelItem(tid, jsonObj["p_info"].toString());
        if (jsonObj.contains(QStringLiteral("p_active")))
            TunnelsDock->SetTunnelActive(tid, jsonObj["p_active"].toBool());
        break;
    }

    case TYPE_TUNNEL_DELETE: {
        qint64 tunnelId = parseI64(jsonObj, "p_tunnel_id");
        TunnelsDock->RemoveTunnelItem(tunnelId);
        if (ClientTunnels.contains(tunnelId)) {
            auto endpoint = ClientTunnels.take(tunnelId);
            endpoint->Stop();
            delete endpoint;
        }
        if (GraphTunnelMarks.contains(tunnelId)) {
            const auto markInfo = GraphTunnelMarks.take(tunnelId);
            QReadLocker locker(&AgentsMapLock);
            if (AgentsMap.contains(markInfo.first)) {
                Agent* agent = AgentsMap.value(markInfo.first, nullptr);
                if (agent && agent->graphItem)
                    agent->graphItem->RemoveTunnel(static_cast<TunnelMarkType>(markInfo.second));
            }
        }
        break;
    }

    case TYPE_BROWSER_DISKS: {
        QReadLocker locker(&AgentsMapLock);
        qint64 agentId = parseI64(jsonObj, "b_agent_id");
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser()) {
                agent->GetFileBrowser()->SetDisksWin( jsonObj["b_time"].toDouble(), jsonObj["b_msg_type"].toDouble(), jsonObj["b_message"].toString(), jsonObj["b_data"].toString() );
            }
        }
        break;
    }

    case TYPE_BROWSER_FILES: {
        QReadLocker locker(&AgentsMapLock);
        qint64 agentId = parseI64(jsonObj, "b_agent_id");
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser()){
                agent->GetFileBrowser()->AddFiles( jsonObj["b_time"].toDouble(), jsonObj["b_msg_type"].toDouble(), jsonObj["b_message"].toString(), jsonObj["b_path"].toString(), jsonObj["b_data"].toString() );
            }
        }
        break;
    }

    case TYPE_BROWSER_PROCESS: {
        QReadLocker locker(&AgentsMapLock);
        qint64 agentId = parseI64(jsonObj, "b_agent_id");
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasProcessBrowser()) {
                agent->GetProcessBrowser()->SetStatus( jsonObj["b_time"].toDouble(), jsonObj["b_msg_type"].toDouble(), jsonObj["b_message"].toString() );
                agent->GetProcessBrowser()->SetProcess( jsonObj["b_msg_type"].toDouble(), jsonObj["b_data"].toString() );
            }
        }
        break;
    }

    case TYPE_BROWSER_STATUS: {
        QReadLocker locker(&AgentsMapLock);
        qint64 agentId = parseI64(jsonObj, "b_agent_id");
        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->HasFileBrowser())
                agent->GetFileBrowser()->SetStatus( jsonObj["b_time"].toDouble(), jsonObj["b_msg_type"].toDouble(), jsonObj["b_message"].toString() );
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

    case TYPE_GROUP_CREATE: {
        int64_t groupId       = parseI64(jsonObj, "g_group_id");
        int64_t parentGroupId = parseI64(jsonObj, "g_parent_group_id");
        QString groupName     = jsonObj["g_group_name"].toString();
        QString scope         = jsonObj["g_scope"].toString();
        QJsonArray membersArr = jsonObj["g_members"].toArray();
        QVector<qint64> members;
        for (const QJsonValue &v : membersArr)
            members.append(parseI64(v));
        if (scope == "agents" && SessionsTableDock)
            SessionsTableDock->OnGroupCreated(groupId, parentGroupId, groupName, members);
        break;
    }

    case TYPE_GROUP_RENAME: {
        int64_t groupId   = parseI64(jsonObj, "g_group_id");
        QString groupName = jsonObj["g_group_name"].toString();
        if (SessionsTableDock)
            SessionsTableDock->OnGroupRenamed(groupId, groupName);
        break;
    }

    case TYPE_GROUP_DELETE: {
        int64_t groupId = parseI64(jsonObj, "g_group_id");
        if (SessionsTableDock)
            SessionsTableDock->OnGroupDeleted(groupId);
        break;
    }

    case TYPE_GROUP_MEMBERS: {
        int64_t groupId    = parseI64(jsonObj, "g_group_id");
        QJsonArray addArr  = jsonObj["g_add"].toArray();
        QJsonArray remArr  = jsonObj["g_remove"].toArray();
        QVector<qint64> add, remove;
        for (const QJsonValue &v : addArr)
            add.append(parseI64(v));
        for (const QJsonValue &v : remArr)
            remove.append(parseI64(v));
        if (SessionsTableDock)
            SessionsTableDock->OnGroupMembersChanged(groupId, add, remove);
        break;
    }

    case TYPE_GROUP_REPARENT: {
        int64_t groupId     = parseI64(jsonObj, "g_group_id");
        int64_t newParentId = parseI64(jsonObj, "g_new_parent_id");
        if (SessionsTableDock)
            SessionsTableDock->OnGroupReparented(groupId, newParentId);
        break;
    }

    case TYPE_AXSCRIPT_COMMANDS:
        this->ProcessAxScriptPacket(jsonObj["name"].toString(), jsonObj["content"].toString(), jsonObj["groups"].toArray());
        break;

    case TYPE_AXSCRIPT_LIST:
        Q_EMIT serverScriptsChanged();
        break;

    case TYPE_EVENT_HANDLERS:
        Q_EMIT eventHandlersChanged();
        break;

    case SP_TYPE_EVENT:
        LogsDock->AddLogs( jsonObj["event_type"].toDouble(), jsonObj["date"].toDouble(), jsonObj["message"].toString() );
        if (LogsDock) {
            auto* coreDw = LogsDock->dock() ? LogsDock->dock()->dockWidget() : nullptr;
            const bool viewingLogs = coreDw && coreDw->isOpen() && coreDw->isCurrentTab();
            if (!viewingLogs)
                LogsUnreadIncrement();
        }
        break;

    case TYPE_LOG_BATCH:
        if (LogsDock)
            LogsDock->AddServerLogBatch( jsonObj["items"].toArray() );
        break;


    default:
        break;
    }
}

void AdaptixWidget::setSyncUpdateUI(const bool enabled)
{
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
}
