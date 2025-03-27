#include <UI/Widgets/AdaptixWidget.h>

bool AdaptixWidget::isValidSyncPacket(QJsonObject jsonObj)
{
    if ( !jsonObj.contains("type") || !jsonObj["type"].isDouble() )
        return false;

    int spType = jsonObj["type"].toDouble();

    if( spType == TYPE_SYNC_START ) {
        if ( !jsonObj.contains("count") || !jsonObj["count"].isDouble() ) return false;
        return true;
    }
    if( spType == TYPE_SYNC_FINISH ) {
        return true;
    }

    if(spType == SP_TYPE_EVENT) {
        if ( !jsonObj.contains("event_type") || !jsonObj["event_type"].isDouble() ) return false;
        if ( !jsonObj.contains("date")       || !jsonObj["date"].isDouble() )       return false;
        if ( !jsonObj.contains("message")    || !jsonObj["message"].isString() )    return false;
        return true;
    }

    if( spType == TYPE_LISTENER_REG ) {
        if ( !jsonObj.contains("fn") || !jsonObj["fn"].isString() ) return false;
        if ( !jsonObj.contains("ui") || !jsonObj["ui"].isString() ) return false;
        return true;
    }
    if( spType == TYPE_LISTENER_START || spType == TYPE_LISTENER_EDIT ) {
        if ( !jsonObj.contains("l_name")       || !jsonObj["l_name"].isString() )       return false;
        if ( !jsonObj.contains("l_type")       || !jsonObj["l_type"].isString() )       return false;
        if ( !jsonObj.contains("l_bind_host")  || !jsonObj["l_bind_host"].isString() )  return false;
        if ( !jsonObj.contains("l_bind_port")  || !jsonObj["l_bind_port"].isString() )  return false;
        if ( !jsonObj.contains("l_agent_host") || !jsonObj["l_agent_host"].isString() ) return false;
        if ( !jsonObj.contains("l_agent_port") || !jsonObj["l_agent_port"].isString() ) return false;
        if ( !jsonObj.contains("l_status")     || !jsonObj["l_status"].isString() )     return false;
        if ( !jsonObj.contains("l_data")       || !jsonObj["l_data"].isString() )       return false;
        return true;
    }
    if( spType == TYPE_LISTENER_STOP ) {
        if ( !jsonObj.contains("l_name") || !jsonObj["l_name"].isString() ) return false;
        return true;
    }

    if( spType == TYPE_AGENT_REG ) {
        if ( !jsonObj.contains("agent")    || !jsonObj["agent"].isString() )    return false;
        if ( !jsonObj.contains("listener") || !jsonObj["listener"].isArray() ) return false;
        if ( !jsonObj.contains("ui")       || !jsonObj["ui"].isString() )       return false;
        if ( !jsonObj.contains("cmd")      || !jsonObj["cmd"].isString() )      return false;
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
    if( spType == TYPE_AGENT_TASK_REMOVE ) {
        if (!jsonObj.contains("a_task_id") || !jsonObj["a_task_id"].isString()) return false;
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
    if( this->sync && dialogSyncPacket != nullptr ) {
        dialogSyncPacket->receivedLogs++;
        dialogSyncPacket->upgrade();
    }

    if( spType == TYPE_SYNC_START )
    {
        int count = jsonObj["count"].toDouble();
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

            emit this->SyncedSignal();
        }
        return;
    }


    if( spType == TYPE_LISTENER_START )
    {
        ListenerData newListener = {0};
        newListener.ListenerName = jsonObj["l_name"].toString();
        newListener.ListenerType = jsonObj["l_type"].toString();
        newListener.BindHost     = jsonObj["l_bind_host"].toString();
        newListener.BindPort     = jsonObj["l_bind_port"].toString();
        newListener.AgentPort    = jsonObj["l_agent_port"].toString();
        newListener.AgentHost    = jsonObj["l_agent_host"].toString();
        newListener.Status       = jsonObj["l_status"].toString();
        newListener.Data         = jsonObj["l_data"].toString();

        ListenersTab->AddListenerItem(newListener);
        return;
    }
    if( spType == TYPE_LISTENER_EDIT )
    {
        ListenerData newListener = {0};
        newListener.ListenerName = jsonObj["l_name"].toString();
        newListener.ListenerType = jsonObj["l_type"].toString();
        newListener.BindHost     = jsonObj["l_bind_host"].toString();
        newListener.BindPort     = jsonObj["l_bind_port"].toString();
        newListener.AgentPort    = jsonObj["l_agent_port"].toString();
        newListener.AgentHost    = jsonObj["l_agent_host"].toString();
        newListener.Status       = jsonObj["l_status"].toString();
        newListener.Data         = jsonObj["l_data"].toString();

        ListenersTab->EditListenerItem(newListener);
        return;
    }
    if( spType == TYPE_LISTENER_STOP )
    {
        QString listenerName = jsonObj["l_name"].toString();

        ListenersTab->RemoveListenerItem(listenerName);
        return;
    }


    if( spType == TYPE_AGENT_NEW )
    {
        QString agentName = jsonObj["a_name"].toString();

        Agent* newAgent = new Agent(jsonObj, RegisterAgentsCmd[agentName], this);
        SessionsTablePage->AddAgentItem( newAgent );
        SessionsGraphPage->AddAgent(newAgent, this->synchronized);
        return;
    }
    if( spType == TYPE_AGENT_UPDATE )
    {
        QString agentId = jsonObj["a_id"].toString();

        if(AgentsMap.contains(agentId)) {
            Agent* agent = AgentsMap[agentId];
            agent->Update(jsonObj);
        }
    }
    if( spType == TYPE_AGENT_TICK )
    {
        QJsonArray agentIDs = jsonObj["a_id"].toArray();
        for (QJsonValue idValue : agentIDs) {
            QString id = idValue.toString();
            if (AgentsMap.contains(id)) {
                Agent* agent = AgentsMap[id];
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
            SessionsGraphPage->RemoveAgent(this->AgentsMap[agentId], this->synchronized);
            SessionsTablePage->RemoveAgentItem(agentId);
            TasksTab->RemoveAgentTasksItem(agentId);

        }
    }



    if( spType == TYPE_AGENT_TASK_SYNC )
    {
        Task* newTask = new Task(jsonObj);
        TasksTab->AddTaskItem(newTask);
        return;
    }
    if( spType == TYPE_AGENT_TASK_UPDATE )
    {
        QString taskId = jsonObj["a_task_id"].toString();

        if(TasksMap.contains(taskId)) {
            Task* task = TasksMap[taskId];
            task->Update(jsonObj);
        }
        return;
    }
    if( spType == TYPE_AGENT_TASK_SEND )
    {
        QJsonArray taskIDs = jsonObj["a_task_id"].toArray();
        for (QJsonValue idValue : taskIDs) {
            QString id = idValue.toString();
            if (TasksMap.contains(id)) {
                Task* task = TasksMap[id];
                task->item_Result->setText("Running");
            }
        }

        return;
    }
    if( spType == TYPE_AGENT_TASK_REMOVE )
    {
        QString TaskId = jsonObj["a_task_id"].toString();
        TasksTab->RemoveTaskItem(TaskId);
        return;
    }



    if( spType == TYPE_AGENT_CONSOLE_OUT )
    {
        qint64  time    = static_cast<qint64>(jsonObj["time"].toDouble());
        QString agentId = jsonObj["a_id"].toString();
        QString text    = jsonObj["a_text"].toString();
        QString message = jsonObj["a_message"].toString();
        int     msgType = jsonObj["a_msg_type"].toDouble();

        if (AgentsMap.contains(agentId))
            AgentsMap[agentId]->Console->ConsoleOutputMessage(time, "", msgType, message, text, false );

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

        if (AgentsMap.contains(AgentId))
            AgentsMap[AgentId]->Console->ConsoleOutputMessage( FinishTime, TaskId, MessageType, Message, Output , Completed );

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

        DownloadsTab->AddDownloadItem(newDownload);

        return;
    }
    if( spType == TYPE_DOWNLOAD_UPDATE )
    {
        QString fileId = jsonObj["d_file_id"].toString();
        int recvSize   = jsonObj["d_recv_size"].toDouble();
        int state      = jsonObj["d_state"].toDouble();

        DownloadsTab->EditDownloadItem(fileId, recvSize, state);

        return;
    }
    if( spType == TYPE_DOWNLOAD_DELETE )
    {
        QString fileId = jsonObj["d_file_id"].toString();

        DownloadsTab->RemoveDownloadItem(fileId);

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

        TunnelsTab->AddTunnelItem(newTunnel);

        return;
    }
    if( spType == TYPE_TUNNEL_EDIT )
    {
        QString TunnelId = jsonObj["p_tunnel_id"].toString();
        QString Info     = jsonObj["p_info"].toString();

        TunnelsTab->EditTunnelItem(TunnelId, Info);

        return;
    }
    if( spType == TYPE_TUNNEL_DELETE )
    {
        QString TunnelId = jsonObj["p_tunnel_id"].toString();

        TunnelsTab->RemoveTunnelItem(TunnelId);

        return;
    }



    if( spType == TYPE_BROWSER_DISKS )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();
        QString data    = jsonObj["b_data"].toString();

        if (AgentsMap.contains(agentId))
            AgentsMap[agentId]->FileBrowser->SetDisks(time, msgType, message, data);

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

        if (AgentsMap.contains(agentId))
            AgentsMap[agentId]->FileBrowser->AddFiles(time, msgType, message, path, data);

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
            AgentsMap[agentId]->ProcessBrowser->SetStatus(time, msgType, message);
            AgentsMap[agentId]->ProcessBrowser->SetProcess(msgType, data);
        }

        return;
    }
    if( spType == TYPE_BROWSER_STATUS )
    {
        QString agentId = jsonObj["b_agent_id"].toString();
        qint64  time    = jsonObj["b_time"].toDouble();
        int     msgType = jsonObj["b_msg_type"].toDouble();
        QString message = jsonObj["b_message"].toString();

        if (AgentsMap.contains(agentId))
            AgentsMap[agentId]->FileBrowser->SetStatus( time, msgType, message );

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

            SessionsGraphPage->RelinkAgent(parentAgent, childAgent, pivotData.PivotName, this->synchronized);
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

                SessionsGraphPage->UnlinkAgent(parentAgent, childAgent, this->synchronized);
            }
        }
    }



    if(spType == SP_TYPE_EVENT)
    {
        int     type    = jsonObj["event_type"].toDouble();
        qint64  time    = jsonObj["date"].toDouble();
        QString message = jsonObj["message"].toString();

        LogsTab->AddLogs(type, time, message);
    }


    if( spType == TYPE_LISTENER_REG )
    {
        QString fn = jsonObj["fn"].toString();
        QString ui = jsonObj["ui"].toString();

        auto widgetBuilder = new WidgetBuilder(ui.toLocal8Bit() );
        if(widgetBuilder->GetError().isEmpty())
            RegisterListenersUI[fn] = widgetBuilder;

        return;
    }
    if( spType == TYPE_AGENT_REG )
    {
        QString agentName = jsonObj["agent"].toString();
        QString ui        = jsonObj["ui"].toString();
        QString cmd       = jsonObj["cmd"].toString();
        QJsonArray listenerNames = jsonObj["listener"].toArray();

        auto widgetBuilder = new WidgetBuilder(ui.toLocal8Bit() );
        if(widgetBuilder->GetError().isEmpty())
            RegisterAgentsUI[agentName] = widgetBuilder;

        auto commander = new Commander();
        bool result = true;
        QString msg = ValidCommandsFile(cmd.toLocal8Bit(), &result);
        if ( result )
            commander->AddRegCommands(cmd.toLocal8Bit());

        RegisterAgentsCmd[agentName] = commander;

        for (QJsonValue listener : listenerNames) {
            QString name = listener.toString();
            LinkListenerAgent[name].push_back(agentName);
        }

        return;
    }
}
