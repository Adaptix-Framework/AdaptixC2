package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
	"sort"

	"github.com/Adaptix-Framework/axc2/v2"
)

const (
	MaxConsoleEntriesPerAgent = 500
	MaxTasksPerAgent          = 500
)

func (ts *Teamserver) TsClientConnected(username string) bool {
	return ts.Broker.ClientExists(username)
}

func getPacketCategory(packet interface{}) string {
	switch packet.(type) {
	case SyncPackerListenerReg, SyncPackerAgentReg, SyncPackerServiceReg, SyncPackerAxScriptData:
		return "extenders"
	case SyncPackerAxScriptList:
		return SyncCategoryScripts
	case SyncPackerListenerStart:
		return "listeners"
	case SyncPackerAgentNew, SyncPackerAgentUpdate:
		return "agents"
	case SyncPackerAgentConsoleOutput, SyncPackerAgentConsoleTaskSync, SyncPackerAgentConsoleTaskUpd:
		return SyncCategoryConsoleHistory
	case SyncPackerAgentTaskSync, SyncPackerAgentTaskUpdate:
		return SyncCategoryTasksHistory
	case SpNotification:
		return "notifications"
	case SyncPackerLogBatch:
		return "logs"
	case SyncPackerChatMessage:
		return SyncCategoryChatHistory
	case SyncPackerChatEdit, SyncPackerChatDelete, SyncPackerChatReaction:
		return SyncCategoryChatRealtime
	case SyncPackerChatTodo:
		return SyncCategoryChatTodo
	case SyncPackerTransferCreate, SyncPackerTransferUpdate, SyncPackerTransferActual:
		return SyncCategoryDownloadsHistory
	case SyncPackerScreenshotCreate:
		return SyncCategoryScreenshotHistory
	case SyncPackerTunnelCreate, SyncPackerTunnelAccept:
		return "tunnels"
	case SyncPackerPivotCreate:
		return "pivots"
	case SyncPackerGroupCreate, SyncPackerGroupRename, SyncPackerGroupDelete, SyncPackerGroupMembers, SyncPackerGroupReparent:
		return "groups"
	case SyncPackerCredentialsAdd:
		return SyncCategoryCredentialsHistory
	case SyncPackerTargetsAdd:
		return SyncCategoryTargetsHistory
	case SyncPackerPayloadCreate, SyncPackerPayloadUpdate, SyncPackerPayloadDelete, SyncPackerPayloadEdit, SyncPackerPayloadTag:
		return SyncCategoryPayloads
	case json.RawMessage:
		return SyncCategoryConsoleHistory
	default:
		return "misc"
	}
}

func (ts *Teamserver) TsSyncClient(username string, packet interface{}) {
	ts.Broker.PublishTo(username, packet)
}

func (ts *Teamserver) TsSyncExcludeClient(username string, packet interface{}) {
	ts.Broker.PublishExclude(username, packet)
}

func (ts *Teamserver) TsSyncAllClients(packet interface{}) {
	ts.Broker.Publish(packet)
}

func (ts *Teamserver) TsSyncAllClientsWithCategory(packet interface{}, category string) {
	ts.Broker.PublishWithCategory(packet, category)
}

func (ts *Teamserver) TsSyncExcludeClientWithCategory(username string, packet interface{}, category string) {
	ts.Broker.PublishExcludeWithCategory(username, packet, category)
}

func (ts *Teamserver) TsSyncConsole(packet interface{}, taskClient string, initiator string) {
	ts.Broker.PublishConsole(packet, taskClient, initiator)
}

func (ts *Teamserver) TsSyncAgentActivated(packet interface{}) {
	ts.Broker.PublishAgentActivated(packet)
}

func (ts *Teamserver) TsSyncState(packet interface{}, stateKey string) {
	ts.Broker.PublishState(packet, stateKey)
}

func (ts *Teamserver) TsSyncStateWithCategory(packet interface{}, stateKey string, category string) {
	ts.Broker.PublishStateWithCategory(packet, stateKey, category)
}

func (ts *Teamserver) TsSyncCategories(client *ClientHandler, categories []string) {
	var packets []interface{}

	requested := make(map[string]bool)
	for _, cat := range categories {
		requested[cat] = true
	}

	if requested[SyncCategoryExtenders] {
		delete(requested, SyncCategoryExtenders)
		packets = append(packets, ts.TsPresyncExtenders()...)
	}
	if requested[SyncCategoryScripts] {
		delete(requested, SyncCategoryScripts)
		packets = append(packets, ts.TsPresyncAxScriptData()...)
		packets = append(packets, ts.TsPresyncAxScriptList())
		packets = append(packets, ts.TsPresyncEventHandlers())
	}
	if requested[SyncCategoryListeners] {
		delete(requested, SyncCategoryListeners)
		packets = append(packets, ts.TsPresyncListeners()...)
	}
	if requested[SyncCategoryAgents] {
		delete(requested, SyncCategoryAgents)
		delete(requested, SyncCategoryAgentsOnlyActive)
		packets = append(packets, ts.TsPresyncAgents()...)
	} else if requested[SyncCategoryAgentsOnlyActive] {
		delete(requested, SyncCategoryAgentsOnlyActive)
		packets = append(packets, ts.TsPresyncAgentsActive()...)
	}

	if requested[SyncCategoryAgentsInactive] {
		delete(requested, SyncCategoryAgentsInactive)
		packets = append(packets, ts.TsPresyncAgentsInactive()...)
	}

	if requested[SyncCategoryPivots] {
		delete(requested, SyncCategoryPivots)
		packets = append(packets, ts.TsPresyncPivots()...)
	}

	if requested[SyncCategoryGroups] {
		delete(requested, SyncCategoryGroups)
		packets = append(packets, ts.TsPresyncGroups()...)
	}

	if requested[SyncCategoryPayloads] {
		delete(requested, SyncCategoryPayloads)
		packets = append(packets, ts.TsPresyncPayloads()...)
	}

	for category := range requested {
		switch category {
		case SyncCategoryTasksHistory:
		case SyncCategoryConsoleHistory:
		case SyncCategoryDownloadsHistory:
		case SyncCategoryScreenshotHistory:
		case SyncCategoryCredentialsHistory:
		case SyncCategoryTargetsHistory:
		case SyncCategoryChatHistory:
			packets = append(packets, ts.TsPresyncChat()...)
		case SyncCategoryChatTodo:
			packets = append(packets, ts.TsPresyncChatTodo()...)
		case SyncCategoryNotifications:
			packets = append(packets, ts.TsPresyncNotifications()...)
		case SyncCategoryTunnels:
			packets = append(packets, ts.TsPresyncTunnels()...)
		}
	}

	ts.sendSyncPackets(client, packets)
}

func (ts *Teamserver) sendSyncPackets(client *ClientHandler, packets []interface{}) {
	const BATCH_SIZE = 500
	estimatedBatches := (len(packets) / BATCH_SIZE) + 1
	serializedPackets := make([][]byte, 0, estimatedBatches)

	categoryMap := make(map[string][]interface{}, 16)
	categoryOrder := make([]string, 0, 16)

	for _, p := range packets {
		category := getPacketCategory(p)
		if _, exists := categoryMap[category]; !exists {
			categoryOrder = append(categoryOrder, category)
		}
		categoryMap[category] = append(categoryMap[category], p)
	}

	for _, category := range categoryOrder {
		categoryPackets := categoryMap[category]

		for i := 0; i < len(categoryPackets); i += BATCH_SIZE {
			end := i + BATCH_SIZE
			if end > len(categoryPackets) {
				end = len(categoryPackets)
			}

			batch := categoryPackets[i:end]
			batchPacket := CreateSpSyncCategoryBatch(category, batch)

			data := serializePacket(batchPacket)
			if data != nil {
				serializedPackets = append(serializedPackets, data)
			}
		}
	}

	startPacket := CreateSpSyncStart(len(serializedPackets), ts.Parameters.Interfaces)
	startData := serializePacket(startPacket)

	finishPacket := CreateSpSyncFinish()
	finishData := serializePacket(finishPacket)

	client.SendSync(startData)

	for _, serialized := range serializedPackets {
		client.SendSync(serialized)
	}

	client.SendSync(finishData)
}

///////////////

func (ts *Teamserver) TsPresyncExtenders() []interface{} {
	totalCount := ts.listener_configs.Len() + ts.agent_configs.Len() + ts.service_configs.Len()
	packets := make([]interface{}, 0, totalCount)

	ts.listener_configs.ForEachFast(func(key string, listenerInfo extender.ListenerInfo) bool {
		p := CreateSpListenerReg(listenerInfo.Name, listenerInfo.Protocol, listenerInfo.Type, listenerInfo.AX)
		packets = append(packets, p)
		return true
	})

	ts.agent_configs.ForEachFast(func(key string, agentInfo extender.AgentInfo) bool {
		groups := ts.TsGetAgentCommandGroups(agentInfo.Name)
		p := CreateSpAgentReg(agentInfo.Name, agentInfo.AX, agentInfo.Listeners, agentInfo.MultiListeners, groups)
		packets = append(packets, p)
		return true
	})

	ts.service_configs.ForEachFast(func(key string, serviceInfo extender.ServiceInfo) bool {
		p := CreateSpServiceReg(serviceInfo.Name, serviceInfo.AX)
		packets = append(packets, p)
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncListeners() []interface{} {
	count := ts.listeners.Len()
	listeners := make([]adaptix.ListenerData, 0, count)
	ts.listeners.ForEachFast(func(key string, listenerData adaptix.ListenerData) bool {
		listeners = append(listeners, listenerData)
		return true
	})

	sort.Slice(listeners, func(i, j int) bool {
		return listeners[i].CreateTime < listeners[j].CreateTime
	})

	packets := make([]interface{}, 0, len(listeners))
	for _, listenerData := range listeners {
		t := CreateSpListenerStart(listenerData)
		packets = append(packets, t)
	}
	return packets
}

func (ts *Teamserver) TsPresyncAgents() []interface{} {
	return ts.presyncAgentsFiltered(false)
}

func (ts *Teamserver) TsPresyncAgentsActive() []interface{} {
	return ts.presyncAgentsFiltered(true)
}

func (ts *Teamserver) TsPresyncAgentsInactive() []interface{} {
	count := ts.Agents.Len()
	agents := make([]adaptix.AgentData, 0, count)

	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		agentData := agent.GetData()
		if !ts.isAgentInactive(agentData.Mark) {
			return true
		}
		agents = append(agents, agentData)
		return true
	})

	sort.Slice(agents, func(i, j int) bool {
		return agents[i].CreateTime < agents[j].CreateTime
	})

	packets := make([]interface{}, 0, len(agents))
	ts.Agents.DirectLock()
	for _, agentData := range agents {
		var groups map[string]bool
		if ag, ok := ts.Agents.Get(agentData.Id); ok {
			groups = ag.GetCommandGroupOverrides()
		}
		p := CreateSpAgentNewWithGroups(agentData, groups)
		packets = append(packets, p)
	}
	ts.Agents.DirectUnlock()

	return packets
}

func (ts *Teamserver) isAgentInactive(mark string) bool {
	return mark == "Inactive" || mark == "Terminated" || mark == "Disconnect"
}

func (ts *Teamserver) presyncAgentsFiltered(activeOnly bool) []interface{} {
	count := ts.Agents.Len()
	agents := make([]adaptix.AgentData, 0, count)

	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		agentData := agent.GetData()
		if activeOnly && ts.isAgentInactive(agentData.Mark) {
			return true
		}
		agents = append(agents, agentData)
		return true
	})

	sort.Slice(agents, func(i, j int) bool {
		return agents[i].CreateTime < agents[j].CreateTime
	})

	packets := make([]interface{}, 0, len(agents))
	ts.Agents.DirectLock()
	for _, agentData := range agents {
		var groups map[string]bool
		if ag, ok := ts.Agents.Get(agentData.Id); ok {
			groups = ag.GetCommandGroupOverrides()
		}
		p := CreateSpAgentNewWithGroups(agentData, groups)
		packets = append(packets, p)
	}
	ts.Agents.DirectUnlock()

	return packets
}

func (ts *Teamserver) TsPresyncConsole(client *ClientHandler) []interface{} {
	var packets []interface{}
	consoleTeamMode := client.ConsoleTeamMode()
	username := client.Username()
	activeOnly := client.IsSubscribed(SyncCategoryAgentsOnlyActive) && !client.IsSubscribed(SyncCategoryAgents)

	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		if activeOnly {
			if ts.isAgentInactive(agent.GetData().Mark) {
				return true
			}
		}
		restoreConsoles := ts.DBMS.DbConsoleAll(key)
		for _, message := range restoreConsoles {
			if !consoleTeamMode {
				var check struct {
					Client string `json:"a_client"`
					TaskId int64  `json:"a_task_id"`
				}
				_ = json.Unmarshal(message, &check)
				if check.TaskId != 0 && check.Client != username {
					continue
				}
			}
			packets = append(packets, json.RawMessage(message))
		}
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncConsoleInactive(client *ClientHandler) []interface{} {
	var packets []interface{}
	consoleTeamMode := client.ConsoleTeamMode()
	username := client.Username()

	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		if !ts.isAgentInactive(agent.GetData().Mark) {
			return true
		}
		restoreConsoles := ts.DBMS.DbConsoleAll(key)
		for _, message := range restoreConsoles {
			if !consoleTeamMode {
				var check struct {
					Client string `json:"a_client"`
					TaskId int64  `json:"a_task_id"`
				}
				_ = json.Unmarshal(message, &check)
				if check.TaskId != 0 && check.Client != username {
					continue
				}
			}
			packets = append(packets, json.RawMessage(message))
		}
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncGroups() []interface{} {
	count := ts.groups.Len()
	packets := make([]interface{}, 0, count)
	ts.groups.ForEachFast(func(key int64, g adaptix.GroupData) bool {
		p := CreateSpGroupCreate(g.GroupId, g.ParentGroupId, g.GroupName, g.Scope, g.Members)
		packets = append(packets, p)
		return true
	})
	return packets
}

func (ts *Teamserver) TsPresyncPivots() []interface{} {
	count := ts.pivots.Len()
	packets := make([]interface{}, 0, count)
	ts.pivots.DirectAccess(func(item interface{}) {
		pivot := item.(*adaptix.PivotData)
		p := CreateSpPivotCreate(*pivot)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncChat() []interface{} {
	dbMessages := ts.DBMS.DbChatRecent(40, 0)
	packets := make([]interface{}, 0, len(dbMessages))
	for _, message := range dbMessages {
		p := CreateSpChatMessageEx(message)
		packets = append(packets, p)
	}
	return packets
}

func (ts *Teamserver) TsPresyncChatTodo() []interface{} {
	content, updatedBy, updatedAt := ts.DBMS.DbChatGetTodo()
	return []interface{}{CreateSpChatTodo(content, updatedBy, updatedAt)}
}

func (ts *Teamserver) TsPresyncTunnels() []interface{} {
	count := 0
	ts.TunnelManager.ForEachTunnel(func(key int64, tunnel *Tunnel) bool {
		count++
		return true
	})
	packets := make([]interface{}, 0, count)
	ts.TunnelManager.ForEachTunnel(func(key int64, tunnel *Tunnel) bool {
		if tunnel == nil {
			return true
		}
		tunnel.mu.RLock()
		d := tunnel.Data
		d.Active = tunnel.Active
		sent := tunnel.BytesSent.Load()
		recv := tunnel.BytesRecv.Load()
		tunnel.mu.RUnlock()
		packets = append(packets, CreateSpTunnelCreate(d, sent, recv))
		return true
	})
	return packets
}

func (ts *Teamserver) TsPresyncNotifications() []interface{} {
	count := ts.notifications.Len()
	packets := make([]interface{}, 0, count)
	ts.notifications.DirectAccess(func(item interface{}) {
		packets = append(packets, item)
	})
	return packets
}
