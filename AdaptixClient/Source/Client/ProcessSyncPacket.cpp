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
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogSyncPacket.h>

bool AdaptixWidget::isValidSyncPacket(QJsonObject jsonObj)
{
    if ( !jsonObj.contains("type") || !jsonObj["type"].isDouble() )
        return false;

    int spType = jsonObj["type"].toDouble();

    if( spType == TYPE_SYNC_START ) {
        if ( !jsonObj.contains("count")      || !jsonObj["count"].isDouble() ) return false;
        if ( !jsonObj.contains("interfaces") || !jsonObj["interfaces"].isArray() )  return false;
        return true;
    }
    if( spType == TYPE_SYNC_FINISH ) {
        return true;
    }

    if( spType == TYPE_SYNC_BATCH ) {
        if ( !jsonObj.contains("packets") || !jsonObj["packets"].isArray() ) return false;
        return true;
    }

    if( spType == TYPE_SYNC_CATEGORY_BATCH ) {
        if ( !jsonObj.contains("category") || !jsonObj["category"].isString() ) return false;
        if ( !jsonObj.contains("packets") || !jsonObj["packets"].isArray() ) return false;
        return true;
    }

    if(spType == SP_TYPE_EVENT) {
        if ( !jsonObj.contains("event_type") || !jsonObj["event_type"].isDouble() ) return false;
        if ( !jsonObj.contains("date")       || !jsonObj["date"].isDouble() )       return false;
        if ( !jsonObj.contains("message")    || !jsonObj["message"].isString() )    return false;
        return true;
    }

    if( spType == TYPE_LISTENER_REG ) {
        if ( !jsonObj.contains("l_name")     || !jsonObj["l_name"].isString() )     return false;
        if ( !jsonObj.contains("l_protocol") || !jsonObj["l_protocol"].isString() ) return false;
        if ( !jsonObj.contains("l_type")     || !jsonObj["l_type"].isString() )     return false;
        if ( !jsonObj.contains("ax")         || !jsonObj["ax"].isString() )         return false;
        return true;
    }
    if( spType == TYPE_LISTENER_START || spType == TYPE_LISTENER_EDIT ) {
        if ( !jsonObj.contains("l_name")       || !jsonObj["l_name"].isString() )       return false;
        if ( !jsonObj.contains("l_reg_name")   || !jsonObj["l_reg_name"].isString() )   return false;
        if ( !jsonObj.contains("l_protocol")   || !jsonObj["l_protocol"].isString() )   return false;
        if ( !jsonObj.contains("l_type")       || !jsonObj["l_type"].isString() )       return false;
        if ( !jsonObj.contains("l_bind_host")  || !jsonObj["l_bind_host"].isString() )  return false;
        if ( !jsonObj.contains("l_bind_port")  || !jsonObj["l_bind_port"].isString() )  return false;
        if ( !jsonObj.contains("l_agent_addr") || !jsonObj["l_agent_addr"].isString() ) return false;
        if ( !jsonObj.contains("l_status")     || !jsonObj["l_status"].isString() )     return false;
        if ( !jsonObj.contains("l_data")       || !jsonObj["l_data"].isString() )       return false;
        return true;
    }
    if( spType == TYPE_LISTENER_STOP ) {
        if ( !jsonObj.contains("l_name") || !jsonObj["l_name"].isString() ) return false;
        return true;
    }

    if( spType == TYPE_AGENT_REG ) {
        if ( !jsonObj.contains("agent")     || !jsonObj["agent"].isString() )     return false;
        if ( !jsonObj.contains("ax")        || !jsonObj["ax"].isString() )        return false;
        if ( !jsonObj.contains("listeners") || !jsonObj["listeners"].isArray() )  return false;
        return true;
    }
    if( spType == TYPE_AGENT_NEW ) {
        if ( !jsonObj.contains("a_id")           || !jsonObj["a_id"].isString() )           return false;
        if ( !jsonObj.contains("a_name")         || !jsonObj["a_name"].isString() )         return false;
        if ( !jsonObj.contains("a_listener")     || !jsonObj["a_listener"].isString() )     return false;
        if ( !jsonObj.contains("a_async")        || !jsonObj["a_async"].isBool() )          return false;
        if ( !jsonObj.contains("a_external_ip")  || !jsonObj["a_external_ip"].isString() )  return false;
        if ( !jsonObj.contains("a_internal_ip")  || !jsonObj["a_internal_ip"].isString() )  return false;
        if ( !jsonObj.contains("a_gmt_offset")   || !jsonObj["a_gmt_offset"].isDouble() )   return false;
        if ( !jsonObj.contains("a_sleep")        || !jsonObj["a_sleep"].isDouble() )        return false;
        if ( !jsonObj.contains("a_jitter")       || !jsonObj["a_jitter"].isDouble() )       return false;
        if ( !jsonObj.contains("a_pid")          || !jsonObj["a_pid"].isString() )          return false;
        if ( !jsonObj.contains("a_tid")          || !jsonObj["a_tid"].isString() )          return false;
        if ( !jsonObj.contains("a_arch")         || !jsonObj["a_arch"].isString() )         return false;
        if ( !jsonObj.contains("a_elevated")     || !jsonObj["a_elevated"].isBool() )       return false;
        if ( !jsonObj.contains("a_process")      || !jsonObj["a_process"].isString() )      return false;
        if ( !jsonObj.contains("a_os")           || !jsonObj["a_os"].isDouble() )           return false;
        if ( !jsonObj.contains("a_os_desc")      || !jsonObj["a_os_desc"].isString() )      return false;
        if ( !jsonObj.contains("a_domain")       || !jsonObj["a_domain"].isString() )       return false;
        if ( !jsonObj.contains("a_computer")     || !jsonObj["a_computer"].isString() )     return false;
        if ( !jsonObj.contains("a_username")     || !jsonObj["a_username"].isString() )     return false;
        if ( !jsonObj.contains("a_impersonated") || !jsonObj["a_impersonated"].isString() ) return false;
        if ( !jsonObj.contains("a_tags")         || !jsonObj["a_tags"].isString() )         return false;
        if ( !jsonObj.contains("a_mark")         || !jsonObj["a_mark"].isString() )         return false;
        if ( !jsonObj.contains("a_color")        || !jsonObj["a_color"].isString() )        return false;
        if ( !jsonObj.contains("a_last_tick")    || !jsonObj["a_last_tick"].isDouble() )    return false;
        return true;
    }
    if( spType == TYPE_AGENT_TICK ) {
        if (!jsonObj.contains("a_id") || !jsonObj["a_id"].isArray()) return false;
        return true;
    }
    if( spType == TYPE_AGENT_UPDATE ) {
        if ( !jsonObj.contains("a_id")           || !jsonObj["a_id"].isString() )           return false;
        if ( !jsonObj.contains("a_sleep")        || !jsonObj["a_sleep"].isDouble() )        return false;
        if ( !jsonObj.contains("a_jitter")       || !jsonObj["a_jitter"].isDouble() )       return false;
        if ( !jsonObj.contains("a_impersonated") || !jsonObj["a_impersonated"].isString() ) return false;
        if ( !jsonObj.contains("a_tags")         || !jsonObj["a_tags"].isString() )         return false;
        if ( !jsonObj.contains("a_mark")         || !jsonObj["a_mark"].isString() )         return false;
        if ( !jsonObj.contains("a_color")        || !jsonObj["a_color"].isString() )        return false;
        return true;
    }
    if( spType == TYPE_AGENT_REMOVE ) {
        if (!jsonObj.contains("a_id") || !jsonObj["a_id"].isString()) return false;
        return true;
    }

    if( spType == TYPE_AGENT_TASK_SYNC ) {
        if (!jsonObj.contains("a_id")          || !jsonObj["a_id"].isString())          return false;
        if (!jsonObj.contains("a_task_id")     || !jsonObj["a_task_id"].isString())     return false;
        if (!jsonObj.contains("a_task_type")   || !jsonObj["a_task_type"].isDouble())   return false;
        if (!jsonObj.contains("a_start_time")  || !jsonObj["a_start_time"].isDouble())  return false;
        if (!jsonObj.contains("a_cmdline")     || !jsonObj["a_cmdline"].isString())     return false;
        if (!jsonObj.contains("a_client")      || !jsonObj["a_client"].isString())      return false;
        if (!jsonObj.contains("a_user")        || !jsonObj["a_user"].isString())        return false;
        if (!jsonObj.contains("a_computer")    || !jsonObj["a_computer"].isString())    return false;
        if (!jsonObj.contains("a_finish_time") || !jsonObj["a_finish_time"].isDouble()) return false;
        if (!jsonObj.contains("a_msg_type")    || !jsonObj["a_msg_type"].isDouble())    return false;
        if (!jsonObj.contains("a_message")     || !jsonObj["a_message"].isString())     return false;
        if (!jsonObj.contains("a_text")        || !jsonObj["a_text"].isString())        return false;
        if (!jsonObj.contains("a_completed")   || !jsonObj["a_completed"].isBool())     return false;
        return true;
    }
    if( spType == TYPE_AGENT_TASK_UPDATE ) {
        if (!jsonObj.contains("a_id")          || !jsonObj["a_id"].isString())          return false;
        if (!jsonObj.contains("a_task_id")     || !jsonObj["a_task_id"].isString())     return false;
        if (!jsonObj.contains("a_task_type")   || !jsonObj["a_task_type"].isDouble())   return false;
        if (!jsonObj.contains("a_finish_time") || !jsonObj["a_finish_time"].isDouble()) return false;
        if (!jsonObj.contains("a_msg_type")    || !jsonObj["a_msg_type"].isDouble())    return false;
        if (!jsonObj.contains("a_message")     || !jsonObj["a_message"].isString())     return false;
        if (!jsonObj.contains("a_text")        || !jsonObj["a_text"].isString())        return false;
        if (!jsonObj.contains("a_completed")   || !jsonObj["a_completed"].isBool())     return false;
        return true;
    }
    if ( spType == TYPE_AGENT_TASK_SEND ) {
        if (!jsonObj.contains("a_task_id") || !jsonObj["a_task_id"].isArray()) return false;
        return true;
    }
    if ( spType == TYPE_AGENT_TASK_REMOVE ) {
        if (!jsonObj.contains("a_task_id") || !jsonObj["a_task_id"].isString()) return false;
        return true;
    }
    if ( spType == TYPE_AGENT_TASK_HOOK ) {
        if (!jsonObj.contains("a_id")        || !jsonObj["a_id"].isString())        return false;
        if (!jsonObj.contains("a_task_id")   || !jsonObj["a_task_id"].isString())   return false;
        if (!jsonObj.contains("a_hook_id")   || !jsonObj["a_hook_id"].isString())   return false;
        if (!jsonObj.contains("a_job_index") || !jsonObj["a_job_index"].isDouble()) return false;
        if (!jsonObj.contains("a_msg_type")  || !jsonObj["a_msg_type"].isDouble())  return false;
        if (!jsonObj.contains("a_message")   || !jsonObj["a_message"].isString())   return false;
        if (!jsonObj.contains("a_text")      || !jsonObj["a_text"].isString())      return false;
        if (!jsonObj.contains("a_completed") || !jsonObj["a_completed"].isBool())   return false;
        return true;
    }

    if( spType == TYPE_AGENT_CONSOLE_OUT) {
        if (!jsonObj.contains("time")       || !jsonObj["time"].isDouble())       return false;
        if (!jsonObj.contains("a_id")       || !jsonObj["a_id"].isString())       return false;
        if (!jsonObj.contains("a_text")     || !jsonObj["a_text"].isString())     return false;
        if (!jsonObj.contains("a_message")  || !jsonObj["a_message"].isString())  return false;
        if (!jsonObj.contains("a_msg_type") || !jsonObj["a_msg_type"].isDouble()) return false;
        return true;
    }
    if( spType == TYPE_AGENT_CONSOLE_TASK_SYNC ) {
        if (!jsonObj.contains("a_id")          || !jsonObj["a_id"].isString())          return false;
        if (!jsonObj.contains("a_task_id")     || !jsonObj["a_task_id"].isString())     return false;
        if (!jsonObj.contains("a_start_time")  || !jsonObj["a_start_time"].isDouble())  return false;
        if (!jsonObj.contains("a_cmdline")     || !jsonObj["a_cmdline"].isString())     return false;
        if (!jsonObj.contains("a_client")      || !jsonObj["a_client"].isString())      return false;
        if (!jsonObj.contains("a_finish_time") || !jsonObj["a_finish_time"].isDouble()) return false;
        if (!jsonObj.contains("a_msg_type")    || !jsonObj["a_msg_type"].isDouble())    return false;
        if (!jsonObj.contains("a_message")     || !jsonObj["a_message"].isString())     return false;
        if (!jsonObj.contains("a_text")        || !jsonObj["a_text"].isString())        return false;
        if (!jsonObj.contains("a_completed")   || !jsonObj["a_completed"].isBool())     return false;
        return true;
    }
    if( spType == TYPE_AGENT_CONSOLE_TASK_UPD ) {
        if (!jsonObj.contains("a_id")          || !jsonObj["a_id"].isString())          return false;
        if (!jsonObj.contains("a_task_id")     || !jsonObj["a_task_id"].isString())     return false;
        if (!jsonObj.contains("a_finish_time") || !jsonObj["a_finish_time"].isDouble()) return false;
        if (!jsonObj.contains("a_msg_type")    || !jsonObj["a_msg_type"].isDouble())    return false;
        if (!jsonObj.contains("a_message")     || !jsonObj["a_message"].isString())     return false;
        if (!jsonObj.contains("a_text")        || !jsonObj["a_text"].isString())        return false;
        if (!jsonObj.contains("a_completed")   || !jsonObj["a_completed"].isBool())     return false;
        return true;
    }

    if( spType == TYPE_CHAT_MESSAGE )
    {
        if (!jsonObj.contains("c_username") || !jsonObj["c_username"].isString()) return false;
        if (!jsonObj.contains("c_message")  || !jsonObj["c_message"].isString())  return false;
        if (!jsonObj.contains("c_date")     || !jsonObj["c_date"].isDouble())     return false;
        return true;
    }

    if( spType == TYPE_DOWNLOAD_CREATE ) {
        if (!jsonObj.contains("d_agent_id")   || !jsonObj["d_agent_id"].isString())   return false;
        if (!jsonObj.contains("d_file_id")    || !jsonObj["d_file_id"].isString())    return false;
        if (!jsonObj.contains("d_agent_name") || !jsonObj["d_agent_name"].isString()) return false;
        if (!jsonObj.contains("d_user")       || !jsonObj["d_user"].isString())       return false;
        if (!jsonObj.contains("d_computer")   || !jsonObj["d_computer"].isString())   return false;
        if (!jsonObj.contains("d_file")       || !jsonObj["d_file"].isString())       return false;
        if (!jsonObj.contains("d_size")       || !jsonObj["d_size"].isDouble())       return false;
        if (!jsonObj.contains("d_date")       || !jsonObj["d_date"].isDouble())       return false;
        return true;
    }
    if( spType == TYPE_DOWNLOAD_UPDATE ) {
        if (!jsonObj.contains("d_file_id")   || !jsonObj["d_file_id"].isString())   return false;
        if (!jsonObj.contains("d_recv_size") || !jsonObj["d_recv_size"].isDouble()) return false;
        if (!jsonObj.contains("d_state")     || !jsonObj["d_state"].isDouble())     return false;
        return true;
    }
    if( spType == TYPE_DOWNLOAD_DELETE ) {
        if (!jsonObj.contains("d_file_id") || !jsonObj["d_file_id"].isString()) return false;
        return true;
    }

    if( spType == TYPE_TUNNEL_CREATE ) {
        if (!jsonObj.contains("p_tunnel_id") || !jsonObj["p_tunnel_id"].isString()) return false;
        if (!jsonObj.contains("p_agent_id")  || !jsonObj["p_agent_id"].isString())  return false;
        if (!jsonObj.contains("p_computer")  || !jsonObj["p_computer"].isString())  return false;
        if (!jsonObj.contains("p_username")  || !jsonObj["p_username"].isString())  return false;
        if (!jsonObj.contains("p_process")   || !jsonObj["p_process"].isString())   return false;
        if (!jsonObj.contains("p_type")      || !jsonObj["p_type"].isString())      return false;
        if (!jsonObj.contains("p_info")      || !jsonObj["p_info"].isString())      return false;
        if (!jsonObj.contains("p_interface") || !jsonObj["p_interface"].isString()) return false;
        if (!jsonObj.contains("p_port")      || !jsonObj["p_port"].isString())      return false;
        if (!jsonObj.contains("p_client")    || !jsonObj["p_client"].isString())    return false;
        if (!jsonObj.contains("p_fport")     || !jsonObj["p_fport"].isString())     return false;
        if (!jsonObj.contains("p_fhost")     || !jsonObj["p_fhost"].isString())     return false;
        return true;
    }
    if( spType == TYPE_TUNNEL_EDIT ) {
        if (!jsonObj.contains("p_tunnel_id") || !jsonObj["p_tunnel_id"].isString()) return false;
        if (!jsonObj.contains("p_info")      || !jsonObj["p_info"].isString())      return false;
        return true;
    }
    if( spType == TYPE_TUNNEL_DELETE ) {
        if (!jsonObj.contains("p_tunnel_id") || !jsonObj["p_tunnel_id"].isString()) return false;
        return true;
    }

    if( spType == TYPE_SCREEN_CREATE ) {
        if (!jsonObj.contains("s_screen_id") || !jsonObj["s_screen_id"].isString()) return false;
        if (!jsonObj.contains("s_user")      || !jsonObj["s_user"].isString())      return false;
        if (!jsonObj.contains("s_computer")  || !jsonObj["s_computer"].isString())  return false;
        if (!jsonObj.contains("s_note")      || !jsonObj["s_note"].isString())      return false;
        if (!jsonObj.contains("s_date")      || !jsonObj["s_date"].isDouble())      return false;
        if (!jsonObj.contains("s_content")   || !jsonObj["s_content"].isString())   return false;
        return true;
    }
    if( spType == TYPE_SCREEN_UPDATE ) {
        if (!jsonObj.contains("s_screen_id") || !jsonObj["s_screen_id"].isString()) return false;
        if (!jsonObj.contains("s_note")      || !jsonObj["s_note"].isString())      return false;
        return true;
    }
    if( spType == TYPE_SCREEN_DELETE ) {
        if (!jsonObj.contains("s_screen_id") || !jsonObj["s_screen_id"].isString()) return false;
        return true;
    }


    if ( spType == TYPE_CREDS_CREATE ) {
        if (!jsonObj.contains("c_creds") || !jsonObj["c_creds"].isArray()) return false;
        return true;
    }
    if ( spType == TYPE_CREDS_EDIT ) {
        if (!jsonObj.contains("c_creds_id") || !jsonObj["c_creds_id"].isString()) return false;
        if (!jsonObj.contains("c_username") || !jsonObj["c_username"].isString()) return false;
        if (!jsonObj.contains("c_password") || !jsonObj["c_password"].isString()) return false;
        if (!jsonObj.contains("c_realm")    || !jsonObj["c_realm"].isString())    return false;
        if (!jsonObj.contains("c_type")     || !jsonObj["c_type"].isString())     return false;
        if (!jsonObj.contains("c_tag")      || !jsonObj["c_tag"].isString())      return false;
        if (!jsonObj.contains("c_storage")  || !jsonObj["c_storage"].isString())  return false;
        if (!jsonObj.contains("c_host")     || !jsonObj["c_host"].isString())     return false;
        return true;
    }
    if ( spType == TYPE_CREDS_DELETE ) {
        if (!jsonObj.contains("c_creds_id") || !jsonObj["c_creds_id"].isArray()) return false;
        return true;
    }
    if ( spType == TYPE_CREDS_SET_TAG ) {
        if (!jsonObj.contains("c_creds_id") || !jsonObj["c_creds_id"].isArray()) return false;
        if (!jsonObj.contains("c_tag")      || !jsonObj["c_tag"].isString())     return false;
        return true;
    }

    if ( spType == TYPE_TARGETS_CREATE ) {
        if (!jsonObj.contains("t_targets") || !jsonObj["t_targets"].isArray()) return false;
        return true;
    }
    if ( spType == TYPE_TARGETS_EDIT ) {
        if (!jsonObj.contains("t_target_id") || !jsonObj["t_target_id"].isString()) return false;
        if (!jsonObj.contains("t_computer")  || !jsonObj["t_computer"].isString())  return false;
        if (!jsonObj.contains("t_domain")    || !jsonObj["t_domain"].isString())    return false;
        if (!jsonObj.contains("t_address")   || !jsonObj["t_address"].isString())   return false;
        if (!jsonObj.contains("t_os")        || !jsonObj["t_os"].isDouble())        return false;
        if (!jsonObj.contains("t_os_desk")   || !jsonObj["t_os_desk"].isString())   return false;
        if (!jsonObj.contains("t_tag")       || !jsonObj["t_tag"].isString())       return false;
        if (!jsonObj.contains("t_info")      || !jsonObj["t_info"].isString())      return false;
        if (!jsonObj.contains("t_date")      || !jsonObj["t_date"].isDouble())      return false;
        if (!jsonObj.contains("t_alive")     || !jsonObj["t_alive"].isBool())       return false;
        if (!jsonObj.contains("t_agents"))       return false;
        return true;
    }
    if ( spType == TYPE_TARGETS_DELETE ) {
        if (!jsonObj.contains("t_target_id") || !jsonObj["t_target_id"].isArray()) return false;
        return true;
    }
    if ( spType == TYPE_TARGETS_SET_TAG ) {
        if (!jsonObj.contains("t_targets_id") || !jsonObj["t_targets_id"].isArray()) return false;
        if (!jsonObj.contains("t_tag")        || !jsonObj["t_tag"].isString())       return false;
        return true;
    }



    if( spType == TYPE_BROWSER_DISKS ) {
        if (!jsonObj.contains("b_agent_id") || !jsonObj["b_agent_id"].isString()) return false;
        if (!jsonObj.contains("b_time")     || !jsonObj["b_time"].isDouble())     return false;
        if (!jsonObj.contains("b_msg_type") || !jsonObj["b_msg_type"].isDouble()) return false;
        if (!jsonObj.contains("b_message")  || !jsonObj["b_message"].isString())  return false;
        if (!jsonObj.contains("b_data")     || !jsonObj["b_data"].isString())     return false;
        return true;
    }
    if( spType == TYPE_BROWSER_FILES ) {
        if (!jsonObj.contains("b_agent_id") || !jsonObj["b_agent_id"].isString()) return false;
        if (!jsonObj.contains("b_time")     || !jsonObj["b_time"].isDouble())     return false;
        if (!jsonObj.contains("b_msg_type") || !jsonObj["b_msg_type"].isDouble()) return false;
        if (!jsonObj.contains("b_message")  || !jsonObj["b_message"].isString())  return false;
        if (!jsonObj.contains("b_path")     || !jsonObj["b_path"].isString())     return false;
        if (!jsonObj.contains("b_data")     || !jsonObj["b_data"].isString())     return false;
        return true;
    }
    if( spType == TYPE_BROWSER_STATUS ) {
        if (!jsonObj.contains("b_agent_id") || !jsonObj["b_agent_id"].isString()) return false;
        if (!jsonObj.contains("b_time")     || !jsonObj["b_time"].isDouble())     return false;
        if (!jsonObj.contains("b_msg_type") || !jsonObj["b_msg_type"].isDouble()) return false;
        if (!jsonObj.contains("b_message")  || !jsonObj["b_message"].isString())  return false;
        return true;
    }
    if( spType == TYPE_BROWSER_PROCESS ) {
        if (!jsonObj.contains("b_agent_id") || !jsonObj["b_agent_id"].isString()) return false;
        if (!jsonObj.contains("b_time")     || !jsonObj["b_time"].isDouble())     return false;
        if (!jsonObj.contains("b_msg_type") || !jsonObj["b_msg_type"].isDouble()) return false;
        if (!jsonObj.contains("b_message")  || !jsonObj["b_message"].isString())  return false;
        if (!jsonObj.contains("b_data")     || !jsonObj["b_data"].isString())     return false;
        return true;
    }

    if( spType == TYPE_PIVOT_CREATE ) {
        if (!jsonObj.contains("p_pivot_id")        || !jsonObj["p_pivot_id"].isString())        return false;
        if (!jsonObj.contains("p_pivot_name")      || !jsonObj["p_pivot_name"].isString())      return false;
        if (!jsonObj.contains("p_parent_agent_id") || !jsonObj["p_parent_agent_id"].isString()) return false;
        if (!jsonObj.contains("p_child_agent_id")  || !jsonObj["p_child_agent_id"].isString())  return false;
        return true;
    }
    if( spType == TYPE_PIVOT_DELETE ) {
        if (!jsonObj.contains("p_pivot_id") || !jsonObj["p_pivot_id"].isString()) return false;
        return true;
    }

    return false;
}

void AdaptixWidget::processSyncPacket(QJsonObject jsonObj)
{
    int spType = jsonObj["type"].toDouble();
    
    if( spType == TYPE_SYNC_BATCH || spType == TYPE_SYNC_CATEGORY_BATCH )
    {
        // Process batch packet: extract all packets and process them
        QJsonArray packetsArray = jsonObj["packets"].toArray();
        QString category = jsonObj.value("category").toString("general");
        
        // Temporarily disable UI updates for performance
        if (LogsDock && LogsDock->GetLogsConsoleTextEdit()) {
            LogsDock->GetLogsConsoleTextEdit()->setUpdatesEnabled(false);
        }
        
        for (const QJsonValue &packetValue : packetsArray) {
            if (packetValue.isObject()) {
                QJsonObject packetObj = packetValue.toObject();
                // Recursively process each packet in the batch
                processSyncPacket(packetObj);
            }
        }
        
        // Re-enable UI updates and refresh once
        if (LogsDock && LogsDock->GetLogsConsoleTextEdit()) {
            LogsDock->GetLogsConsoleTextEdit()->setUpdatesEnabled(true);
        }
        
        // Update progress once for entire batch
        if( this->sync && dialogSyncPacket != nullptr ) {
            dialogSyncPacket->receivedLogs += packetsArray.size();
            dialogSyncPacket->upgrade();
        }
        
        return;
    }
    
    if( this->sync && dialogSyncPacket != nullptr && spType != TYPE_SYNC_BATCH ) {
        dialogSyncPacket->receivedLogs++;
        dialogSyncPacket->upgrade();
    }

    if( spType == TYPE_SYNC_START )
    {
        int count = jsonObj["count"].toDouble();
        QJsonArray interfaces = jsonObj["interfaces"].toArray();

        this->addresses.clear();
        for (QJsonValue addrValue : interfaces) {
            QString addr = addrValue.toString();
            this->addresses.append(addr);
        }

        dialogSyncPacket->init(count);
        this->sync = true;
        this->setEnabled(false);
        return;
    }
    if( spType == TYPE_SYNC_FINISH )
    {
        if (dialogSyncPacket != nullptr ) {
            dialogSyncPacket->finish();
            this->sync = false;
            this->setEnabled(true);

            Q_EMIT this->SyncedSignal();
        }
        return;
    }


    if( spType == TYPE_LISTENER_START )
    {
        ListenerData newListener = {};
        newListener.Name             = jsonObj["l_name"].toString();
        newListener.ListenerRegName  = jsonObj["l_reg_name"].toString();
        newListener.ListenerType     = jsonObj["l_type"].toString();
        newListener.ListenerProtocol = jsonObj["l_protocol"].toString();
        newListener.BindHost         = jsonObj["l_bind_host"].toString();
        newListener.BindPort         = jsonObj["l_bind_port"].toString();
        newListener.AgentAddresses   = jsonObj["l_agent_addr"].toString();
        newListener.Status           = jsonObj["l_status"].toString();
        newListener.Data             = jsonObj["l_data"].toString();

        ListenersDock->AddListenerItem(newListener);
        return;
    }
    if( spType == TYPE_LISTENER_EDIT )
    {
        ListenerData newListener = {};
        newListener.Name             = jsonObj["l_name"].toString();
        newListener.ListenerRegName  = jsonObj["l_reg_name"].toString();
        newListener.ListenerType     = jsonObj["l_type"].toString();
        newListener.ListenerProtocol = jsonObj["l_protocol"].toString();
        newListener.BindHost         = jsonObj["l_bind_host"].toString();
        newListener.BindPort         = jsonObj["l_bind_port"].toString();
        newListener.AgentAddresses   = jsonObj["l_agent_addr"].toString();
        newListener.Status           = jsonObj["l_status"].toString();
        newListener.Data             = jsonObj["l_data"].toString();

        ListenersDock->EditListenerItem(newListener);
        return;
    }
    if( spType == TYPE_LISTENER_STOP )
    {
        QString listenerName = jsonObj["l_name"].toString();

        ListenersDock->RemoveListenerItem(listenerName);
        return;
    }


    if( spType == TYPE_AGENT_NEW )
    {
        QString agentName = jsonObj["a_name"].toString();

        Agent* newAgent = new Agent(jsonObj, this);
        SessionsTableDock->AddAgentItem( newAgent );
        SessionsGraphDock->AddAgent(newAgent, this->synchronized);

        if (synchronized)
            Q_EMIT eventNewAgent(newAgent->data.Id);

        return;
    }
    if( spType == TYPE_AGENT_UPDATE )
    {
        QString agentId = jsonObj["a_id"].toString();

        Agent* agent = AgentsMap.value(agentId, nullptr);
        if(agent) {
            auto oldData = agent->data;
            agent->Update(jsonObj);

            SessionsTableDock->UpdateAgentItem(oldData, agent);
        }
        return;
    }
    if( spType == TYPE_AGENT_TICK )
    {
        QJsonArray agentIDs = jsonObj["a_id"].toArray();
        for (QJsonValue idValue : agentIDs) {
            QString id = idValue.toString();

            Agent* agent = AgentsMap.value(id, nullptr);
            if (agent) {
                agent->data.LastTick = QDateTime::currentSecsSinceEpoch();
                if (agent->data.Mark != "Terminated")
                    agent->MarkItem("");
            }
        }
        return;
    }
    if( spType == TYPE_AGENT_REMOVE )
    {
        QString agentId = jsonObj["a_id"].toString();
        if (this->AgentsMap.contains(agentId)) {
            SessionsGraphDock->RemoveAgent(this->AgentsMap[agentId], this->synchronized);
            SessionsTableDock->RemoveAgentItem(agentId);
            TasksDock->RemoveAgentTasksItem(agentId);
        }
        return;
    }



    if( spType == TYPE_AGENT_TASK_SYNC )
    {
        TaskData newTask = {};
        newTask.TaskId      = jsonObj["a_task_id"].toString();
        newTask.TaskType    = jsonObj["a_task_type"].toDouble();
        newTask.AgentId     = jsonObj["a_id"].toString();
        newTask.StartTime   = jsonObj["a_start_time"].toDouble();
        newTask.CommandLine = jsonObj["a_cmdline"].toString();
        newTask.Client      = jsonObj["a_client"].toString();
        newTask.User        = jsonObj["a_user"].toString();
        newTask.Computer    = jsonObj["a_computer"].toString();
        newTask.FinishTime  = jsonObj["a_finish_time"].toDouble();
        newTask.MessageType = jsonObj["a_msg_type"].toDouble();
        newTask.Message     = jsonObj["a_message"].toString();
        newTask.Output      = jsonObj["a_text"].toString();
        newTask.Completed   = jsonObj["a_completed"].toBool();

        TasksDock->AddTaskItem(newTask);
        return;
    }
    if( spType == TYPE_AGENT_TASK_UPDATE )
    {
        QString taskId = jsonObj["a_task_id"].toString();

        if(TasksMap.contains(taskId)) {
            TaskData* task = &TasksMap[taskId];

            task->Completed = jsonObj["a_completed"].toBool();
            if (task->Completed) {
                task->FinishTime = jsonObj["a_finish_time"].toDouble();

                task->MessageType = jsonObj["a_msg_type"].toDouble();
                if ( task->MessageType == CONSOLE_OUT_ERROR || task->MessageType == CONSOLE_OUT_LOCAL_ERROR ) {
                    task->Status = "Error";
                }
                else if ( task->MessageType == CONSOLE_OUT_INFO || task->MessageType == CONSOLE_OUT_LOCAL_INFO ) {
                    task->Status = "Canceled";
                }
                else {
                    task->Status = "Success";
                }
            }

            if ( task->Message.isEmpty() )
                task->Message = jsonObj["a_message"].toString();
            task->Output += jsonObj["a_text"].toString();

            TasksDock->UpdateTaskItem(taskId, *task);
        }
        return;
    }
    if( spType == TYPE_AGENT_TASK_SEND )
    {
        QJsonArray taskIDs = jsonObj["a_task_id"].toArray();
        for (QJsonValue idValue : taskIDs) {
            QString id = idValue.toString();
            if (TasksMap.contains(id)) {
                TaskData* task = &TasksMap[id];
                task->Status = "Running";
            }
        }
        return;
    }
    if( spType == TYPE_AGENT_TASK_REMOVE )
    {
        QString TaskId = jsonObj["a_task_id"].toString();
        TasksDock->RemoveTaskItem(TaskId);
        return;
    }
    if ( spType == TYPE_AGENT_TASK_HOOK )
    {
        this->PostHookProcess(jsonObj);
    }



    if( spType == TYPE_AGENT_CONSOLE_OUT )
    {
        qint64  time    = static_cast<qint64>(jsonObj["time"].toDouble());
        QString agentId = jsonObj["a_id"].toString();
        QString text    = jsonObj["a_text"].toString();
        QString message = jsonObj["a_message"].toString();
        int     msgType = jsonObj["a_msg_type"].toDouble();

        if (AgentsMap.contains(agentId)) {
            AgentsMap[agentId]->Console->ConsoleOutputMessage(time, "", msgType, message, text, false );
        }
        return;
    }
    if( spType == TYPE_AGENT_CONSOLE_TASK_SYNC )
    {
        QString AgentId     = jsonObj["a_id"].toString();
        QString TaskId      = jsonObj["a_task_id"].toString();
        qint64  StartTime   = jsonObj["a_start_time"].toDouble();
        QString CommandLine = jsonObj["a_cmdline"].toString();
        QString Client      = jsonObj["a_client"].toString();
        qint64  FinishTime  = jsonObj["a_finish_time"].toDouble();
        int     MessageType = jsonObj["a_msg_type"].toDouble();
        QString Message     = jsonObj["a_message"].toString();
        QString Output      = jsonObj["a_text"].toString();
        bool    Completed   = jsonObj["a_completed"].toBool();

        if (AgentsMap.contains(AgentId)) {
            AgentsMap[AgentId]->Console->ConsoleOutputPrompt( StartTime, TaskId, Client, CommandLine);
            if (!this->synchronized)
                AgentsMap[AgentId]->Console->AddToHistory(CommandLine);

            qint64 ConsoleTime = StartTime;
            if (Completed)
                ConsoleTime = FinishTime;

            AgentsMap[AgentId]->Console->ConsoleOutputMessage( ConsoleTime, TaskId, MessageType, Message, Output , Completed );
        }
        return;
    }
    if( spType == TYPE_AGENT_CONSOLE_TASK_UPD )
    {
        QString AgentId     = jsonObj["a_id"].toString();
        QString TaskId      = jsonObj["a_task_id"].toString();
        qint64  FinishTime  = jsonObj["a_finish_time"].toDouble();
        int     MessageType = jsonObj["a_msg_type"].toDouble();
        QString Message     = jsonObj["a_message"].toString();
        QString Output      = jsonObj["a_text"].toString();
        bool    Completed   = jsonObj["a_completed"].toBool();

        if (AgentsMap.contains(AgentId)) {
            AgentsMap[AgentId]->Console->ConsoleOutputMessage( FinishTime, TaskId, MessageType, Message, Output , Completed );
        }
        return;
    }



    if( spType == TYPE_CHAT_MESSAGE )
    {
        QString username = jsonObj["c_username"].toString();
        QString message  = jsonObj["c_message"].toString();
        qint64  time     = jsonObj["c_date"].toDouble();
        ChatDock->AddChatMessage(time, username, message);
        return;
    }



    if( spType == TYPE_DOWNLOAD_CREATE )
    {
        DownloadData newDownload = {0};
        newDownload.AgentId   = jsonObj["d_agent_id"].toString();
        newDownload.FileId    = jsonObj["d_file_id"].toString();
        newDownload.AgentName = jsonObj["d_agent_name"].toString();
        newDownload.User      = jsonObj["d_user"].toString();
        newDownload.Computer  = jsonObj["d_computer"].toString();
        newDownload.Filename  = jsonObj["d_file"].toString();
        newDownload.TotalSize = jsonObj["d_size"].toDouble();
        newDownload.Date      = UnixTimestampGlobalToStringLocal(static_cast<qint64>(jsonObj["d_date"].toDouble()));
        newDownload.RecvSize  = 0;
        newDownload.State     = DOWNLOAD_STATE_RUNNING;

        DownloadsDock->AddDownloadItem(newDownload);

        return;
    }
    if( spType == TYPE_DOWNLOAD_UPDATE )
    {
        QString fileId = jsonObj["d_file_id"].toString();
        int recvSize   = jsonObj["d_recv_size"].toDouble();
        int state      = jsonObj["d_state"].toDouble();

        DownloadsDock->EditDownloadItem(fileId, recvSize, state);
        return;
    }
    if( spType == TYPE_DOWNLOAD_DELETE )
    {
        QString fileId = jsonObj["d_file_id"].toString();

        DownloadsDock->RemoveDownloadItem(fileId);
        return;
    }

    if( spType == TYPE_SCREEN_CREATE )
    {
        ScreenData newScreen = {};
        newScreen.ScreenId = jsonObj["s_screen_id"].toString();
        newScreen.User     = jsonObj["s_user"].toString();
        newScreen.Computer = jsonObj["s_computer"].toString();
        newScreen.Note     = jsonObj["s_note"].toString();
        newScreen.Date     = UnixTimestampGlobalToStringLocal(static_cast<qint64>(jsonObj["s_date"].toDouble()));
        newScreen.Content  = QByteArray::fromBase64(jsonObj["s_content"].toString().toUtf8());

        ScreenshotsDock->AddScreenshotItem(newScreen);
        return;
    }
    if( spType == TYPE_SCREEN_UPDATE )
    {
        QString screenId = jsonObj["s_screen_id"].toString();
        QString note     = jsonObj["s_note"].toString();

        ScreenshotsDock->EditScreenshotItem(screenId, note);
        return;
    }
    if( spType == TYPE_SCREEN_DELETE )
    {
        QString screenId = jsonObj["s_screen_id"].toString();
        ScreenshotsDock->RemoveScreenshotItem(screenId);
        return;
    }


    if ( spType == TYPE_CREDS_CREATE ) {
        QList<CredentialData> credList;
        QJsonArray arr = jsonObj.value("c_creds").toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isObject())
                continue;

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
            c.CredId   = obj.value("c_creds_id").toString();
            c.Username = obj.value("c_username").toString();
            c.Password = obj.value("c_password").toString();
            c.Realm    = obj.value("c_realm").toString();
            c.Type     = obj.value("c_type").toString();
            c.Tag      = obj.value("c_tag").toString();
            c.Date     = UnixTimestampGlobalToStringLocal(static_cast<qint64>(obj["c_date"].toDouble()));
            c.Storage  = obj.value("c_storage").toString();
            c.AgentId  = obj.value("c_agent_id").toString();
            c.Host     = obj.value("c_host").toString();

            credList.append(c);
        }

        CredentialsDock->AddCredentialsItems(credList);
        return;
    }
    if ( spType == TYPE_CREDS_EDIT ) {
        CredentialData newCredential = {};
        newCredential.CredId   = jsonObj["c_creds_id"].toString();
        newCredential.Username = jsonObj["c_username"].toString();
        newCredential.Password = jsonObj["c_password"].toString();
        newCredential.Realm    = jsonObj["c_realm"].toString();
        newCredential.Type     = jsonObj["c_type"].toString();
        newCredential.Tag      = jsonObj["c_tag"].toString();
        newCredential.Storage  = jsonObj["c_storage"].toString();
        newCredential.Host     = jsonObj["c_host"].toString();

        CredentialsDock->EditCredentialsItem(newCredential);
        return;
    }
    if ( spType == TYPE_CREDS_DELETE ) {
        QStringList credsId;
        QJsonArray arr = jsonObj.value("c_creds_id").toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isString())
                continue;

            credsId.append(val.toString());
        }

        CredentialsDock->RemoveCredentialsItem(credsId);
        return;
    }
    if ( spType == TYPE_CREDS_SET_TAG ) {
        QString tag    = jsonObj["c_tag"].toString();
        QJsonArray arr = jsonObj["c_creds_id"].toArray();

        QStringList ids;
        for (auto jsonCred : arr) {
            QString id = jsonCred.toString();
            ids.append(id);
        }
        CredentialsDock->CredsSetTag(ids, tag);

        return;
    }


    if ( spType == TYPE_TARGETS_CREATE )
    {
        QList<TargetData> targetsList;
        QJsonArray arr = jsonObj.value("t_targets").toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isObject())
                continue;

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
            t.TargetId = obj.value("t_target_id").toString();
            t.Computer = obj.value("t_computer").toString();
            t.Domain   = obj.value("t_domain").toString();
            t.Address  = obj.value("t_address").toString();
            t.Os       = obj.value("t_os").toInt();
            t.OsDesc   = obj.value("t_os_desk").toString();
            t.Tag      = obj.value("t_tag").toString();
            t.Info     = obj.value("t_info").toString();
            t.Date     = UnixTimestampGlobalToStringLocal(static_cast<qint64>(obj["t_date"].toDouble()));
            t.Alive    = obj.value("t_alive").toBool();

            if (obj.value("t_agents").isArray()) {
                QJsonArray sessions = obj.value("t_agents").toArray();
                for (const QJsonValue &agent_id : sessions) {
                    if (agent_id.isString()) {
                        t.Agents.append(agent_id.toString());
                    }
                }
            }

            bool owned = (t.Agents.size() > 0);
            if (t.Os == OS_WINDOWS) {
                if (owned)
                    t.OsIcon = QIcon(":/icons/os_win_red");
                else if(t.Alive)
                    t.OsIcon = QIcon(":/icons/os_win_blue");
                else
                    t.OsIcon = QIcon(":/icons/os_win_grey");
            }
            else if (t.Os == OS_LINUX) {
                if (owned)
                    t.OsIcon = QIcon(":/icons/os_linux_red");
                else if(t.Alive)
                    t.OsIcon = QIcon(":/icons/os_linux_blue");
                else
                    t.OsIcon = QIcon(":/icons/os_linux_grey");
            }
            else if (t.Os == OS_MAC) {
                if (owned)
                    t.OsIcon = QIcon(":/icons/os_mac_red");
                else if(t.Alive)
                    t.OsIcon = QIcon(":/icons/os_mac_blue");
                else
                    t.OsIcon = QIcon(":/icons/os_mac_grey");
            }

            targetsList.append(t);
        }

        TargetsDock->AddTargetsItems(targetsList);
        return;
    }
    if ( spType == TYPE_TARGETS_EDIT ) {
        TargetData t = {};
        t.TargetId = jsonObj["t_target_id"].toString();
        t.Computer = jsonObj["t_computer"].toString();
        t.Domain   = jsonObj["t_domain"].toString();
        t.Address  = jsonObj["t_address"].toString();
        t.Os       = jsonObj["t_os"].toDouble();
        t.OsDesc   = jsonObj["t_os_desk"].toString();
        t.Tag      = jsonObj["t_tag"].toString();
        t.Info     = jsonObj["t_info"].toString();
        t.Date     = UnixTimestampGlobalToStringLocal(static_cast<qint64>(jsonObj["t_date"].toDouble()));
        t.Alive    = jsonObj["t_alive"].toBool();

        if (jsonObj.value("t_agents").isArray()) {
            QJsonArray sessions = jsonObj.value("t_agents").toArray();
            for (const QJsonValue &agent_id : sessions) {
                if (agent_id.isString()) {
                    t.Agents.append(agent_id.toString());
                }
            }
        }

        bool owned = (t.Agents.size() > 0);
        if (t.Os == OS_WINDOWS) {
            if (owned)
                t.OsIcon = QIcon(":/icons/os_win_red");
            else if(t.Alive)
                t.OsIcon = QIcon(":/icons/os_win_blue");
            else
                t.OsIcon = QIcon(":/icons/os_win_grey");
        }
        else if (t.Os == OS_LINUX) {
            if (owned)
                t.OsIcon = QIcon(":/icons/os_linux_red");
            else if(t.Alive)
                t.OsIcon = QIcon(":/icons/os_linux_blue");
            else
                t.OsIcon = QIcon(":/icons/os_linux_grey");
        }
        else if (t.Os == OS_MAC) {
            if (owned)
                t.OsIcon = QIcon(":/icons/os_mac_red");
            else if(t.Alive)
                t.OsIcon = QIcon(":/icons/os_mac_blue");
            else
                t.OsIcon = QIcon(":/icons/os_mac_grey");
        }

        TargetsDock->EditTargetsItem(t);
        return;
    }
    if ( spType == TYPE_TARGETS_DELETE ) {
        QStringList targetsId;
        QJsonArray arr = jsonObj.value("t_target_id").toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isString())
                continue;

            targetsId.append(val.toString());
        }

        TargetsDock->RemoveTargetsItem(targetsId);
        return;
    }
    if ( spType == TYPE_TARGETS_SET_TAG ) {
        QString tag    = jsonObj["t_tag"].toString();
        QJsonArray arr = jsonObj["t_targets_id"].toArray();

        QStringList ids;
        for (auto jsonTarget : arr) {
            QString id = jsonTarget.toString();
            ids.append(id);
        }
        TargetsDock->TargetsSetTag(ids, tag);

        return;
    }


    if( spType == TYPE_TUNNEL_CREATE )
    {
        TunnelData newTunnel = {0};
        newTunnel.TunnelId  = jsonObj["p_tunnel_id"].toString();
        newTunnel.AgentId   = jsonObj["p_agent_id"].toString();
        newTunnel.Computer  = jsonObj["p_computer"].toString();
        newTunnel.Username  = jsonObj["p_username"].toString();
        newTunnel.Process   = jsonObj["p_process"].toString();
        newTunnel.Type      = jsonObj["p_type"].toString();
        newTunnel.Info      = jsonObj["p_info"].toString();
        newTunnel.Interface = jsonObj["p_interface"].toString();
        newTunnel.Port      = jsonObj["p_port"].toString();
        newTunnel.Client    = jsonObj["p_client"].toString();
        newTunnel.Fhost     = jsonObj["p_fhost"].toString();
        newTunnel.Fport     = jsonObj["p_fport"].toString();

        TunnelsDock->AddTunnelItem(newTunnel);
        return;
    }
    if( spType == TYPE_TUNNEL_EDIT )
    {
        QString TunnelId = jsonObj["p_tunnel_id"].toString();
        QString Info     = jsonObj["p_info"].toString();

        TunnelsDock->EditTunnelItem(TunnelId, Info);
        return;
    }
    if( spType == TYPE_TUNNEL_DELETE )
    {
        QString TunnelId = jsonObj["p_tunnel_id"].toString();

        TunnelsDock->RemoveTunnelItem(TunnelId);
        return;
    }



    if( spType == TYPE_BROWSER_DISKS )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();
        QString data    = jsonObj["b_data"].toString();

        if (AgentsMap.contains(agentId) ) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->FileBrowser)
                agent->FileBrowser->SetDisksWin(time, msgType, message, data);
        }
        return;
    }
    if( spType == TYPE_BROWSER_FILES )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();
        QString path    = jsonObj["b_path"].toString();
        QString data    = jsonObj["b_data"].toString();

        if (AgentsMap.contains(agentId) ) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->FileBrowser)
                agent->FileBrowser->AddFiles(time, msgType, message, path, data);
        }
        return;
    }
    if( spType == TYPE_BROWSER_PROCESS )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();
        QString data    = jsonObj["b_data"].toString();

        if (AgentsMap.contains(agentId)) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->ProcessBrowser) {
                agent->ProcessBrowser->SetStatus(time, msgType, message);
                agent->ProcessBrowser->SetProcess(msgType, data);
            }
        }
        return;
    }
    if( spType == TYPE_BROWSER_STATUS )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();

        if (AgentsMap.contains(agentId) ) {
            auto agent = AgentsMap[agentId];
            if (agent && agent->FileBrowser)
                agent->FileBrowser->SetStatus(time, msgType, message);
        }
        return;
    }



    if( spType == TYPE_PIVOT_CREATE )
    {
        PivotData pivotData = {};
        pivotData.PivotId       = jsonObj["p_pivot_id"].toString();
        pivotData.PivotName     = jsonObj["p_pivot_name"].toString();
        pivotData.ParentAgentId = jsonObj["p_parent_agent_id"].toString();
        pivotData.ChildAgentId  = jsonObj["p_child_agent_id"].toString();

        if( AgentsMap.contains(pivotData.ParentAgentId) && AgentsMap.contains(pivotData.ChildAgentId) ) {
            Agent* parentAgent = AgentsMap[pivotData.ParentAgentId];
            Agent* childAgent  = AgentsMap[pivotData.ChildAgentId];

            parentAgent->AddChild(pivotData);
            childAgent->SetParent(pivotData);

            Pivots[pivotData.PivotId] = pivotData;

            SessionsGraphDock->RelinkAgent(parentAgent, childAgent, pivotData.PivotName, this->synchronized);
            SessionsTableDock->UpdateAgentItem(childAgent->data, childAgent);
        }
        return;
    }
    if (spType == TYPE_PIVOT_DELETE)
    {
        QString pivotId = jsonObj["p_pivot_id"].toString();

        if (Pivots.contains(pivotId)) {
            PivotData pivotData = Pivots[pivotId];
            Pivots.remove(pivotId);

            if( AgentsMap.contains(pivotData.ParentAgentId) && AgentsMap.contains(pivotData.ChildAgentId) ) {
                Agent* parentAgent = AgentsMap[pivotData.ParentAgentId];
                Agent* childAgent  = AgentsMap[pivotData.ChildAgentId];

                parentAgent->RemoveChild(pivotData);
                childAgent->UnsetParent(pivotData);

                SessionsGraphDock->UnlinkAgent(parentAgent, childAgent, this->synchronized);
                SessionsTableDock->UpdateAgentItem(childAgent->data, childAgent);
            }
        }
        return;
    }



    if(spType == SP_TYPE_EVENT)
    {
        int     type    = jsonObj["event_type"].toDouble();
        qint64  time    = jsonObj["date"].toDouble();
        QString message = jsonObj["message"].toString();

        LogsDock->AddLogs(type, time, message);
        return;
    }



    if( spType == TYPE_LISTENER_REG )
    {
        QString name     = jsonObj["l_name"].toString();
        QString protocol = jsonObj["l_protocol"].toString();
        QString type     = jsonObj["l_type"].toString();
        QString ax       = jsonObj["ax"].toString();

        this->RegisterListenerConfig(name, protocol, type, ax);
        return;
    }
    if( spType == TYPE_AGENT_REG )
    {
        QString agentName         = jsonObj["agent"].toString();
        QString ax_script         = jsonObj["ax"].toString();
        QJsonArray listenersArray = jsonObj["listeners"].toArray();

        QStringList listeners;
        for (QJsonValue listener : listenersArray)
            listeners.append(listener.toString());

        this->RegisterAgentConfig(agentName, ax_script, listeners);
        return;
    }
}
