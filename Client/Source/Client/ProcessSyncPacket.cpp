#include <UI/Widgets/AdaptixWidget.h>

bool AdaptixWidget::isValidSyncPacket(QJsonObject jsonObj)
{
    if ( !jsonObj.contains("type") || !jsonObj["type"].isDouble() )
        return false;

    int spType = jsonObj["type"].toDouble();


    if( spType == TYPE_SYNC_START ) {
        if ( !jsonObj.contains("count") || !jsonObj["count"].isDouble() ) {
            return false;
        }
        return true;
    }
    if( spType == TYPE_SYNC_FINISH ) {
        return true;
    }

    if( spType == TYPE_CLIENT_CONNECT || spType == TYPE_CLIENT_DISCONNECT ) {
        if ( !jsonObj.contains("username") || !jsonObj["username"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("time") || !jsonObj["time"].isDouble() ) {
            return false;
        }
        return true;
    }


    if( spType == TYPE_LISTENER_REG ) {
        if ( !jsonObj.contains("fn") || !jsonObj["fn"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("ui") || !jsonObj["ui"].isString() ) {
            return false;
        }
        return true;
    }
    if( spType == TYPE_LISTENER_START || spType == TYPE_LISTENER_EDIT ) {
        if( spType == TYPE_LISTENER_START ) {
            if ( !jsonObj.contains("time") || !jsonObj["time"].isDouble() ) {
                return false;
            }
        }
        if ( !jsonObj.contains("l_name") || !jsonObj["l_name"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_type") || !jsonObj["l_type"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_bind_host") || !jsonObj["l_bind_host"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_bind_port") || !jsonObj["l_bind_port"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_agent_host") || !jsonObj["l_agent_host"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_agent_port") || !jsonObj["l_agent_port"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_status") || !jsonObj["l_status"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("l_data") || !jsonObj["l_data"].isString() ) {
            return false;
        }

        return true;
    }
    if( spType == TYPE_LISTENER_STOP ) {
        if ( !jsonObj.contains("l_name") || !jsonObj["l_name"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("time") || !jsonObj["time"].isDouble() ) {
            return false;
        }

        return true;
    }

    if( spType == TYPE_AGENT_REG ) {
        if ( !jsonObj.contains("agent") || !jsonObj["agent"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("listener") || !jsonObj["listener"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("ui") || !jsonObj["ui"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("cmd") || !jsonObj["cmd"].isString() ) {
            return false;
        }
        return true;
    }

    if( spType == TYPE_AGENT_NEW ) {
        if ( !jsonObj.contains("time") || !jsonObj["time"].isDouble() ) {
            return false;
        }
        if ( !jsonObj.contains("a_id") || !jsonObj["a_id"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_name") || !jsonObj["a_name"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_listener") || !jsonObj["a_listener"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_async") || !jsonObj["a_async"].isBool() ) {
            return false;
        }
        if ( !jsonObj.contains("a_external_ip") || !jsonObj["a_external_ip"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_internal_ip") || !jsonObj["a_internal_ip"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_gmt_offset") || !jsonObj["a_gmt_offset"].isDouble() ) {
            return false;
        }
        if ( !jsonObj.contains("a_sleep") || !jsonObj["a_sleep"].isDouble() ) {
            return false;
        }
        if ( !jsonObj.contains("a_jitter") || !jsonObj["a_jitter"].isDouble() ) {
            return false;
        }
        if ( !jsonObj.contains("a_pid") || !jsonObj["a_pid"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_tid") || !jsonObj["a_tid"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_arch") || !jsonObj["a_arch"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_elevated") || !jsonObj["a_elevated"].isBool() ) {
            return false;
        }
        if ( !jsonObj.contains("a_process") || !jsonObj["a_process"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_os") || !jsonObj["a_os"].isDouble() ) {
            return false;
        }
        if ( !jsonObj.contains("a_os_desc") || !jsonObj["a_os_desc"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_domain") || !jsonObj["a_domain"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_computer") || !jsonObj["a_computer"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_username") || !jsonObj["a_username"].isString() ) {
            return false;
        }
        if ( !jsonObj.contains("a_tags") || !jsonObj["a_tags"].isArray() ) {
            return false;
        }
        if ( !jsonObj.contains("a_last_tick") || !jsonObj["a_last_tick"].isDouble() ) {
            return false;
        }
        return true;
    }

    if( spType == TYPE_AGENT_TICK ) {
        if (!jsonObj.contains("a_id") || !jsonObj["a_id"].isString()) {
            return false;
        }
        return true;
    }

    return false;
}

void AdaptixWidget::processSyncPacket(QJsonObject jsonObj)
{
    int spType = jsonObj["type"].toDouble();
    if( dialogSyncPacket != nullptr ) {
        dialogSyncPacket->receivedLogs++;
        dialogSyncPacket->upgrade();
    }


    if( spType == TYPE_SYNC_START)
    {
        int count = jsonObj["count"].toDouble();
        dialogSyncPacket = new DialogSyncPacket(count);
        return;
    }
    if ( spType == TYPE_SYNC_FINISH)
    {
        if (dialogSyncPacket != nullptr ) {
            dialogSyncPacket->finish();
            delete dialogSyncPacket;
            dialogSyncPacket = nullptr;
        }
        return;
    }


    if( spType == TYPE_CLIENT_CONNECT )
    {
        QString username = jsonObj["username"].toString();
        qint64 time      = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message  = QString("Client '%1' connected to teamserver").arg(username);

        LogsTab->AddLogs(spType, time, message);
        return;
    }
    if ( spType == TYPE_CLIENT_DISCONNECT )
    {
        QString username = jsonObj["username"].toString();
        qint64  time     = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message  = QString("Client '%1' disconnected from teamserver").arg(username);

        LogsTab->AddLogs(spType, time, message);
        return;
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
    if ( spType == TYPE_LISTENER_START )
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

        qint64  time    = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message = QString("Listener '%1' (%2) started").arg(newListener.ListenerName).arg(newListener.ListenerType);

        LogsTab->AddLogs(spType, time, message);

        ListenersTab->AddListenerItem(newListener);
        return;
    }
    if ( spType == TYPE_LISTENER_EDIT )
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

        qint64  time    = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message = QString("Listener '%1' reconfigured").arg(newListener.ListenerName);

        LogsTab->AddLogs(spType, time, message);

        ListenersTab->EditListenerItem(newListener);
        return;
    }
    if ( spType == TYPE_LISTENER_STOP )
    {
        QString listenerName = jsonObj["l_name"].toString();
        qint64  time         = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message      = QString("Listener '%1' stopped").arg(listenerName);

        LogsTab->AddLogs(spType, time, message);

        ListenersTab->RemoveListenerItem(listenerName);
        return;
    }

    if( spType == TYPE_AGENT_REG )
    {
        QString agentName    = jsonObj["agent"].toString();
        QString listenerName = jsonObj["listener"].toString();
        QString ui           = jsonObj["ui"].toString();
        QString cmd          = jsonObj["cmd"].toString();

        auto widgetBuilder = new WidgetBuilder(ui.toLocal8Bit() );
        if(widgetBuilder->GetError().isEmpty())
            RegisterAgentsUI[agentName] = widgetBuilder;

        auto commander = new Commander(cmd.toLocal8Bit());
        if ( commander->IsValid())
            RegisterAgentsCmd[agentName] = commander;

        LinkListenerAgent[listenerName].push_back(agentName);
        return;
    }
    if( spType == TYPE_AGENT_NEW )
    {
        QString agentName = jsonObj["a_name"].toString();

        Agent* newAgent = new Agent(jsonObj, RegisterAgentsCmd[agentName]);

        qint64  time    = static_cast<qint64>(jsonObj["time"].toDouble());
        QString message = QString("New '%1' (%2) executed on '%3@%4.%5' (%6)").arg(
                newAgent->data.Name, newAgent->data.Id, newAgent->data.Username, newAgent->data.Computer, newAgent->data.Domain, newAgent->data.InternalIP
                );

        LogsTab->AddLogs(spType, time, message);

        SessionsTablePage->AddAgentItem( newAgent );
        return;
    }
    if( spType == TYPE_AGENT_TICK )
    {
        QString id = jsonObj["a_id"].toString();
        if (Agents.contains(id))
            Agents[id]->data.LastTick = QDateTime::currentSecsSinceEpoch();
    }
}