package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/safe"
	"sort"

	"github.com/Adaptix-Framework/axc2"
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
	case SyncPackerListenerReg, SyncPackerListenerStart:
		return "listeners"
	case SyncPackerAgentReg, SyncPackerAgentNew, SyncPackerAgentUpdate:
		return "agents"
	case SyncPackerAgentConsoleOutput, SyncPackerAgentConsoleTaskSync, SyncPackerAgentConsoleTaskUpd:
		return SyncCategoryConsoleHistory
	case SyncPackerAgentTaskSync, SyncPackerAgentTaskUpdate:
		return SyncCategoryTasksHistory
	case SpNotification:
		return "notifications"
	case SyncPackerChatMessage:
		return SyncCategoryChatHistory
	case SyncPackerDownloadCreate, SyncPackerDownloadUpdate:
		return SyncCategoryDownloadsHistory
	case SyncPackerScreenshotCreate:
		return SyncCategoryScreenshotHistory
	case SyncPackerTunnelCreate:
		return "tunnels"
	case SyncPackerPivotCreate:
		return "pivots"
	case SyncPackerCredentialsAdd:
		return SyncCategoryCredentialsHistory
	case SyncPackerTargetsAdd:
		return SyncCategoryTargetsHistory
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

func (ts *Teamserver) TsSyncConsole(packet interface{}, taskClient string) {
	ts.Broker.PublishConsole(packet, taskClient)
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
		if client.IsSubscribed(SyncCategoryConsoleHistory) {
			packets = append(packets, ts.TsPresyncConsoleInactive(client)...)
		}
	}

	if requested[SyncCategoryPivots] {
		delete(requested, SyncCategoryPivots)
		packets = append(packets, ts.TsPresyncPivots()...)
	}

	for category := range requested {
		switch category {
		case SyncCategoryTasksHistory:
			presync := ts.TsPresyncTasks()
			if client.IsSubscribed(SyncCategoryTasksOnlyJobs) {
				for i := 0; i < len(presync); i++ {
					p, ok := presync[i].(SyncPackerAgentTaskSync)
					if ok {
						if p.TaskType == adaptix.TASK_TYPE_JOB {
							packets = append(packets, p)
						}
					}
				}
			} else {
				packets = append(packets, presync...)
			}
		case SyncCategoryConsoleHistory:
			packets = append(packets, ts.TsPresyncConsole(client)...)
		case SyncCategoryDownloadsHistory:
			packets = append(packets, ts.TsPresyncDownloads()...)
		case SyncCategoryScreenshotHistory:
			packets = append(packets, ts.TsPresyncScreenshots()...)
		case SyncCategoryCredentialsHistory:
			packets = append(packets, ts.TsPresyncCredentials()...)
		case SyncCategoryTargetsHistory:
			packets = append(packets, ts.TsPresyncTargets()...)
		case SyncCategoryChatHistory:
			packets = append(packets, ts.TsPresyncChat()...)
		case SyncCategoryNotifications:
			packets = append(packets, ts.TsPresyncNotifications()...)
		case SyncCategoryTunnels:
			packets = append(packets, ts.TsPresyncTunnels()...)
		}
	}

	ts.sendSyncPackets(client, packets)
}

//	packets = append(packets, ts.TsPresyncExtenders()...)
//	packets = append(packets, ts.TsPresyncListeners()...)
//	packets = append(packets, ts.TsPresyncAgents()...)
//	packets = append(packets, ts.TsPresyncChat()...)
//	packets = append(packets, ts.TsPresyncDownloads()...)
//	packets = append(packets, ts.TsPresyncScreenshots()...)
//	packets = append(packets, ts.TsPresyncTunnels()...)
//	packets = append(packets, ts.TsPresyncNotifications()...)
//	packets = append(packets, ts.TsPresyncPivots()...)
//	packets = append(packets, ts.TsPresyncCredentials()...)
//	packets = append(packets, ts.TsPresyncTargets()...)

func (ts *Teamserver) sendSyncPackets(client *ClientHandler, packets []interface{}) {
	const BATCH_SIZE = 500
	estimatedBatches := (len(packets) / BATCH_SIZE) + 1
	serializedPackets := make([][]byte, 0, estimatedBatches)

	if !client.VersionSupport() {
		serializedPackets = make([][]byte, 0, len(packets))
		for _, p := range packets {
			data := serializePacket(p)
			if data != nil {
				serializedPackets = append(serializedPackets, data)
			}
		}
	} else {
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

	ts.listener_configs.ForEach(func(key string, value interface{}) bool {
		listenerInfo := value.(extender.ListenerInfo)
		p := CreateSpListenerReg(listenerInfo.Name, listenerInfo.Protocol, listenerInfo.Type, listenerInfo.AX)
		packets = append(packets, p)
		return true
	})

	ts.agent_configs.ForEach(func(key string, value interface{}) bool {
		agentInfo := value.(extender.AgentInfo)
		p := CreateSpAgentReg(agentInfo.Name, agentInfo.AX, agentInfo.Listeners, agentInfo.MultiListeners)
		packets = append(packets, p)
		return true
	})

	ts.service_configs.ForEach(func(key string, value interface{}) bool {
		serviceInfo := value.(extender.ServiceInfo)
		p := CreateSpServiceReg(serviceInfo.Name, serviceInfo.AX)
		packets = append(packets, p)
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncListeners() []interface{} {
	count := ts.listeners.Len()
	listeners := make([]adaptix.ListenerData, 0, count)
	ts.listeners.ForEach(func(key string, value interface{}) bool {
		listenerData := value.(adaptix.ListenerData)
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
	agents := make([]*Agent, 0, count)

	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}
		agentData := agent.GetData()
		if !ts.isAgentInactive(agentData.Mark) {
			return true
		}
		agents = append(agents, agent)
		return true
	})

	sort.Slice(agents, func(i, j int) bool {
		return agents[i].GetData().CreateTime < agents[j].GetData().CreateTime
	})

	packets := make([]interface{}, 0, len(agents))
	ts.Agents.DirectLock()
	for _, agent := range agents {
		if agent != nil {
			p := CreateSpAgentNew(agent.GetData())
			packets = append(packets, p)
		}
	}
	ts.Agents.DirectUnlock()

	return packets
}

func (ts *Teamserver) isAgentInactive(mark string) bool {
	return mark == "Inactive" || mark == "Terminated" || mark == "Disconnect"
}

func (ts *Teamserver) presyncAgentsFiltered(activeOnly bool) []interface{} {
	count := ts.Agents.Len()
	agents := make([]*Agent, 0, count)

	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}
		agentData := agent.GetData()
		if activeOnly && ts.isAgentInactive(agentData.Mark) {
			return true
		}
		agents = append(agents, agent)
		return true
	})

	sort.Slice(agents, func(i, j int) bool {
		return agents[i].GetData().CreateTime < agents[j].GetData().CreateTime
	})

	packets := make([]interface{}, 0, len(agents))
	ts.Agents.DirectLock()
	for _, agent := range agents {
		if agent != nil {
			p := CreateSpAgentNew(agent.GetData())
			packets = append(packets, p)
		}
	}
	ts.Agents.DirectUnlock()

	return packets
}

func (ts *Teamserver) TsPresyncTasks() []interface{} {
	var sortedTasks []adaptix.TaskData

	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}
		agent.CompletedTasks.ForEach(func(key2 string, value2 interface{}) bool {
			taskData := value2.(adaptix.TaskData)
			sortedTasks = append(sortedTasks, taskData)
			return true
		})
		return true
	})

	sort.Slice(sortedTasks, func(i, j int) bool {
		return sortedTasks[i].StartDate < sortedTasks[j].StartDate
	})

	packets := make([]interface{}, 0, len(sortedTasks))
	for _, taskData := range sortedTasks {
		t := CreateSpAgentTaskSync(taskData)
		packets = append(packets, t)
	}

	return packets
}

func (ts *Teamserver) TsPresyncConsole(client *ClientHandler) []interface{} {
	var packets []interface{}
	consoleTeamMode := client.ConsoleTeamMode()
	username := client.Username()
	activeOnly := client.IsSubscribed(SyncCategoryAgentsOnlyActive) && !client.IsSubscribed(SyncCategoryAgents)

	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}
		if activeOnly {
			agentData := agent.GetData()
			if ts.isAgentInactive(agentData.Mark) {
				return true
			}
		}
		agent.OutConsole.DirectAccess(func(item interface{}) {
			if !consoleTeamMode {
				if consoleTask, ok := item.(SyncPackerAgentConsoleTaskSync); ok {
					if consoleTask.Client != username {
						return
					}
				}
			}
			packets = append(packets, item)
		})
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncConsoleInactive(client *ClientHandler) []interface{} {
	var packets []interface{}
	consoleTeamMode := client.ConsoleTeamMode()
	username := client.Username()

	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}
		agentData := agent.GetData()
		if !ts.isAgentInactive(agentData.Mark) {
			return true
		}
		agent.OutConsole.DirectAccess(func(item interface{}) {
			if !consoleTeamMode {
				if consoleTask, ok := item.(SyncPackerAgentConsoleTaskSync); ok {
					if consoleTask.Client != username {
						return
					}
				}
			}
			packets = append(packets, item)
		})
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
	count := ts.messages.Len()
	packets := make([]interface{}, 0, count)
	ts.messages.DirectAccess(func(item interface{}) {
		message := item.(adaptix.ChatData)
		p := CreateSpChatMessage(message)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncDownloads() []interface{} {
	count := ts.downloads.Len()
	sortedDownloads := make([]adaptix.DownloadData, 0, count)

	ts.downloads.ForEach(func(key string, value interface{}) bool {
		downloadData := value.(adaptix.DownloadData)
		sortedDownloads = append(sortedDownloads, downloadData)
		return true
	})

	sort.Slice(sortedDownloads, func(i, j int) bool {
		return sortedDownloads[i].Date < sortedDownloads[j].Date
	})

	packets := make([]interface{}, 0, len(sortedDownloads)*2)
	for _, downloadData := range sortedDownloads {
		d1 := CreateSpDownloadCreate(downloadData)
		d2 := CreateSpDownloadUpdate(downloadData)
		packets = append(packets, d1, d2)
	}

	return packets
}

func (ts *Teamserver) TsPresyncScreenshots() []interface{} {
	count := ts.screenshots.Len()
	sortedScreens := make([]adaptix.ScreenData, 0, count)

	ts.screenshots.ForEach(func(key string, value interface{}) bool {
		screenData := value.(adaptix.ScreenData)
		sortedScreens = append(sortedScreens, screenData)
		return true
	})

	sort.Slice(sortedScreens, func(i, j int) bool {
		return sortedScreens[i].Date < sortedScreens[j].Date
	})

	packets := make([]interface{}, 0, len(sortedScreens))
	for _, screenData := range sortedScreens {
		t := CreateSpScreenshotCreate(screenData)
		packets = append(packets, t)
	}
	return packets
}

func (ts *Teamserver) TsPresyncCredentials() []interface{} {
	count := ts.credentials.Len()
	creds := make([]*adaptix.CredsData, 0, count)
	ts.credentials.DirectAccess(func(item interface{}) {
		c := item.(*adaptix.CredsData)
		creds = append(creds, c)
	})

	if len(creds) == 0 {
		return nil
	}
	p := CreateSpCredentialsAdd(creds)
	return []interface{}{p}
}

func (ts *Teamserver) TsPresyncTargets() []interface{} {
	count := ts.targets.Len()
	targets := make([]*adaptix.TargetData, 0, count)
	ts.targets.DirectAccess(func(item interface{}) {
		target := item.(*adaptix.TargetData)
		targets = append(targets, target)
	})

	if len(targets) == 0 {
		return nil
	}
	p := CreateSpTargetsAdd(targets)
	return []interface{}{p}
}

func (ts *Teamserver) TsPresyncTasksOnlyJobs() []interface{} {
	var packets []interface{}

	ts.Agents.ForEach(func(agentId string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if !ok {
			return true
		}

		agent.RunningJobs.ForEach(func(taskId string, jobsValue interface{}) bool {
			jobs, ok := jobsValue.(*safe.Slice)
			if !ok {
				return true
			}

			jobs.DirectLock()
			slice := jobs.DirectSlice()
			for i := 0; i < len(slice); i++ {
				hookJob, ok := slice[i].(*HookJob)
				if !ok || hookJob == nil {
					continue
				}
				packets = append(packets, CreateSpAgentTaskUpdate(hookJob.Job))
			}
			jobs.DirectUnlock()

			return true
		})

		_ = agentId
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncTunnels() []interface{} {
	count := int(ts.TunnelManager.stats.ActiveTunnels.Load())
	packets := make([]interface{}, 0, count)
	ts.TunnelManager.ForEachTunnel(func(key string, tunnel *Tunnel) bool {
		t := CreateSpTunnelCreate(tunnel.Data)
		packets = append(packets, t)
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
