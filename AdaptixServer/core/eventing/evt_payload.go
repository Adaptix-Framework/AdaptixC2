package eventing

func EventToMap(event any) map[string]any {
	if event == nil {
		return map[string]any{}
	}
	if m, ok := event.(map[string]any); ok {
		return cloneMapShallow(m)
	}

	facts := ExtractFacts(event)
	m := factsToHandlerMap(facts)

	if be := getBaseEvent(event); be != nil {
		et := string(be.Type)
		if et != "" {
			m["type"] = et
			m["event"] = et
		} else if facts.Type != "" {
			m["type"] = facts.Type
			m["event"] = facts.Type
		}
		m["phase"] = phaseName(be.Phase)
	} else if facts.Type != "" {
		m["type"] = facts.Type
		m["event"] = facts.Type
	}

	attachHandlerExtras(event, m, facts)
	return m
}

func factsToHandlerMap(facts EventFacts) map[string]any {
	m := make(map[string]any, 24)

	if facts.HasAgent && facts.AgentID != 0 {
		m["agentId"] = facts.AgentID
	}
	if facts.AgentName != "" {
		m["agentName"] = facts.AgentName
	}
	if facts.User != "" {
		m["user"] = facts.User
	}
	if facts.Listener != "" {
		m["listener"] = facts.Listener
	}
	if facts.ListenerType != "" {
		m["listenerType"] = facts.ListenerType
	}
	if facts.OS != "" {
		m["os"] = facts.OS
	}
	if facts.Computer != "" {
		m["computer"] = facts.Computer
	}
	if facts.Tags != "" {
		m["tags"] = facts.Tags
	}
	if facts.HasClient && facts.Client != "" {
		m["client"] = facts.Client
		// client.connect templates also read username
		if _, ok := m["username"]; !ok {
			m["username"] = facts.Client
		}
	}
	if facts.HasTask && facts.TaskID != 0 {
		m["taskId"] = facts.TaskID
	}
	if facts.HasFile && facts.FileID != 0 {
		m["fileId"] = facts.FileID
	}
	if facts.Filename != "" {
		m["filename"] = facts.Filename
	}
	if facts.HasPort {
		m["port"] = facts.Port
	}
	if facts.TunnelType != "" {
		m["tunnelType"] = facts.TunnelType
	}
	if facts.Realm != "" {
		m["realm"] = facts.Realm
	}
	if facts.CredType != "" {
		m["credType"] = facts.CredType
	}
	if facts.Host != "" {
		m["host"] = facts.Host
	}
	if facts.Domain != "" {
		m["domain"] = facts.Domain
	}
	if facts.Address != "" {
		m["address"] = facts.Address
	}
	if facts.HasAlive {
		m["alive"] = facts.Alive
	}

	if facts.HasAgent || facts.AgentName != "" || facts.Computer != "" || facts.User != "" {
		agent := map[string]any{}
		if facts.HasAgent && facts.AgentID != 0 {
			agent["a_id"] = facts.AgentID
		}
		if facts.AgentName != "" {
			agent["a_name"] = facts.AgentName
		}
		if facts.Listener != "" {
			agent["a_listener"] = facts.Listener
		}
		if facts.Computer != "" {
			agent["a_computer"] = facts.Computer
		}
		if facts.User != "" {
			agent["a_username"] = facts.User
		}
		if facts.Tags != "" {
			agent["a_tags"] = facts.Tags
		}
		if len(agent) > 0 {
			m["agent"] = agent
		}
	}

	if facts.HasTask && facts.TaskID != 0 {
		task := map[string]any{"t_task_id": facts.TaskID}
		if facts.HasAgent && facts.AgentID != 0 {
			task["t_agent_id"] = facts.AgentID
		}
		if facts.HasClient && facts.Client != "" {
			task["t_client"] = facts.Client
		}
		m["task"] = task
	}

	return m
}

func attachHandlerExtras(event any, m map[string]any, facts EventFacts) {
	switch e := event.(type) {
	case *EventDataAgentNew:
		m["restore"] = e.Restore
	case *EventDataAgentGenerate:
		if e.BuilderId != "" {
			m["builderId"] = e.BuilderId
		}
		if e.FileName != "" {
			m["fileName"] = e.FileName
			if _, ok := m["filename"]; !ok {
				m["filename"] = e.FileName
			}
		}
		if len(e.ListenersName) > 0 {
			m["listenersName"] = append([]string(nil), e.ListenersName...)
		}
	case *EventDataTaskCreate:
		if e.Cmdline != "" {
			m["cmdline"] = e.Cmdline
		}
	case *EventDataDownloadStart:
		if e.FileSize != 0 {
			m["fileSize"] = e.FileSize
		}
	case *EventDataDownloadFinish:
		m["canceled"] = e.Canceled
		if e.Download.RemotePath != "" {
			m["remotePath"] = e.Download.RemotePath
		}
		if e.Download.ArtifactName != "" {
			m["artifactName"] = e.Download.ArtifactName
		}
	case *EventDataDownloadRemove:
		if len(e.FileIds) > 0 {
			m["fileIds"] = append([]int64(nil), e.FileIds...)
			m["count"] = len(e.FileIds)
		}
	case *EventDataUploadStart:
		if e.RemotePath != "" {
			m["remotePath"] = e.RemotePath
		}
		if e.FileSize != 0 {
			m["fileSize"] = e.FileSize
		}
	case *EventDataUploadFinish:
		m["canceled"] = e.Canceled
	case *EventDataUploadRemove:
		if len(e.FileIds) > 0 {
			m["fileIds"] = append([]int64(nil), e.FileIds...)
			m["count"] = len(e.FileIds)
		}
	case *EventDataScreenshotAdd:
		if e.Note != "" {
			m["note"] = e.Note
		}
		// intentionally omit Content
	case *EventDataScreenshotRemove:
		m["screenId"] = e.ScreenId
	case *EventDataTunnelStart:
		m["tunnelId"] = e.TunnelId
		if e.Info != "" {
			m["info"] = e.Info
		}
	case *EventDataTunnelStop:
		m["tunnelId"] = e.TunnelId
	case *EventDataClientConnect:
		m["username"] = e.Username
	case *EventDataClientDisconnect:
		m["username"] = e.Username
	case *EventCredentialsAdd:
		m["count"] = len(e.Credentials)
	case *EventCredentialsEdit:
		m["credId"] = e.CredId
	case *EventCredentialsRemove:
		if len(e.CredIds) > 0 {
			m["credIds"] = append([]int64(nil), e.CredIds...)
			m["count"] = len(e.CredIds)
		}
	case *EventDataTargetAdd:
		m["count"] = len(e.Targets)
	case *EventDataTargetEdit:
		m["targetId"] = e.Target.TargetId
	case *EventDataTargetRemove:
		if len(e.TargetIds) > 0 {
			m["targetIds"] = append([]int64(nil), e.TargetIds...)
			m["count"] = len(e.TargetIds)
		}
	case *EventDataPivotCreate:
		m["pivotId"] = e.PivotId
		m["parentAgentId"] = e.ParentAgentId
		m["childAgentId"] = e.ChildAgentId
		if e.PivotName != "" {
			m["pivotName"] = e.PivotName
		}
	case *EventDataPivotRemove:
		m["pivotId"] = e.PivotId
	case *EventDataListenerStart:
		if e.ListenerName != "" {
			m["listenerName"] = e.ListenerName
		}
	case *EventDataListenerStop:
		if e.ListenerName != "" {
			m["listenerName"] = e.ListenerName
		}
	case *EventDataAgentTerminate:
	default:
		_ = e
		_ = facts
	}
}
