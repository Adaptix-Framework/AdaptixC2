package server

import (
	"AdaptixServer/core/extender"
	"bytes"
	"encoding/json"
	"fmt"
	"sort"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientConnected(username string) bool {
	_, found := ts.clients.Get(username)
	return found
}

// getPacketCategory categorizes packets for optimized batch transmission
func getPacketCategory(packet interface{}) string {
	switch packet.(type) {
	case SyncPackerListenerReg, SyncPackerListenerStart:
		return "listeners"
	case SyncPackerAgentReg, SyncPackerAgentNew, SyncPackerAgentUpdate:
		return "agents"
	case SyncPackerAgentConsoleOutput, SyncPackerAgentConsoleTaskSync, SyncPackerAgentConsoleTaskUpd:
		return "console"
	case SyncPackerAgentTaskSync, SyncPackerAgentTaskUpdate:
		return "tasks"
	case SpEvent:
		return "events"
	case SyncPackerChatMessage:
		return "chat"
	case SyncPackerDownloadCreate, SyncPackerDownloadUpdate:
		return "downloads"
	case SyncPackerScreenshotCreate:
		return "screenshots"
	case SyncPackerTunnelCreate:
		return "tunnels"
	case SyncPackerPivotCreate:
		return "pivots"
	case SyncPackerCredentialsAdd:
		return "credentials"
	case SyncPackerTargetsAdd:
		return "targets"
	default:
		return "misc"
	}
}

func (ts *Teamserver) TsSyncClient(username string, packet interface{}) {
	var buffer bytes.Buffer
	err := json.NewEncoder(&buffer).Encode(packet)
	if err != nil {
		return
	}

	value, found := ts.clients.Get(username)
	if found {
		client := value.(*Client)
		client.lockSocket.Lock()
		err = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		client.lockSocket.Unlock()
		if err != nil {
			return
		}
	}
}

func (ts *Teamserver) TsSyncAllClients(packet interface{}) {
	var (
		buffer bytes.Buffer
		err    error
	)

	err = json.NewEncoder(&buffer).Encode(packet)
	if err != nil {
		return
	}
	data := buffer.Bytes()

	ts.clients.ForEach(func(key string, value interface{}) bool {
		client := value.(*Client)
		if client.synced {
			client.lockSocket.Lock()
			_ = client.socket.WriteMessage(websocket.BinaryMessage, data)
			client.lockSocket.Unlock()
		} else {
			client.tmp_store.Put(packet)
		}
		return true
	})
}

func (ts *Teamserver) TsSyncStored(client *Client) {
	var (
		buffer  bytes.Buffer
		packets []interface{}
	)

	startTime := time.Now()

	packets = append(packets, ts.TsPresyncExtenders()...)
	packets = append(packets, ts.TsPresyncListeners()...)
	packets = append(packets, ts.TsPresyncAgents()...)
	packets = append(packets, ts.TsPresyncChat()...)
	packets = append(packets, ts.TsPresyncDownloads()...)
	packets = append(packets, ts.TsPresyncScreenshots()...)
	packets = append(packets, ts.TsPresyncTunnels()...)
	packets = append(packets, ts.TsPresyncEvents()...)
	packets = append(packets, ts.TsPresyncPivots()...)
	packets = append(packets, ts.TsPresyncCredentials()...)
	packets = append(packets, ts.TsPresyncTargets()...)

	client.lockSocket.Lock()
	defer client.lockSocket.Unlock()

	startPacket := CreateSpSyncStart(len(packets), ts.Parameters.Interfaces)
	_ = json.NewEncoder(&buffer).Encode(startPacket)
	_ = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()

	// Backward compatibility: check if client supports batch sync
	if !client.supportsBatchSync {
		// Legacy mode: send individual packets for old clients (v0.10 and earlier)
		for _, p := range packets {
			var pBuffer bytes.Buffer
			_ = json.NewEncoder(&pBuffer).Encode(p)
			_ = client.socket.WriteMessage(websocket.BinaryMessage, pBuffer.Bytes())
		}

		finishPacket := CreateSpSyncFinish()
		_ = json.NewEncoder(&buffer).Encode(finishPacket)
		_ = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		buffer.Reset()

		elapsed := time.Since(startTime)
		println(fmt.Sprintf("[SYNC] Client %s (legacy): %d packets (individual), completed in %v",
			client.username, len(packets), elapsed))
		return
	}

	// Phase 2: Type-based categorized batching for better organization
	// Group packets by category and send in optimized batches
	const BATCH_SIZE = 100

	categoryMap := make(map[string][]interface{})
	categoryOrder := []string{} // Preserve order

	// Categorize packets by type
	for _, p := range packets {
		category := getPacketCategory(p)
		if _, exists := categoryMap[category]; !exists {
			categoryOrder = append(categoryOrder, category)
		}
		categoryMap[category] = append(categoryMap[category], p)
	}

	// Send categorized batches
	for _, category := range categoryOrder {
		categoryPackets := categoryMap[category]

		// Send in batches within each category
		for i := 0; i < len(categoryPackets); i += BATCH_SIZE {
			end := i + BATCH_SIZE
			if end > len(categoryPackets) {
				end = len(categoryPackets)
			}

			batch := categoryPackets[i:end]
			batchPacket := CreateSpSyncCategoryBatch(category, batch)

			var pBuffer bytes.Buffer
			_ = json.NewEncoder(&pBuffer).Encode(batchPacket)
			_ = client.socket.WriteMessage(websocket.BinaryMessage, pBuffer.Bytes())
		}
	}

	finishPacket := CreateSpSyncFinish()
	_ = json.NewEncoder(&buffer).Encode(finishPacket)
	_ = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()

	elapsed := time.Since(startTime)
	totalBatches := 0
	for _, catPackets := range categoryMap {
		totalBatches += (len(catPackets) + BATCH_SIZE - 1) / BATCH_SIZE
	}
	println(fmt.Sprintf("[SYNC] Client %s: %d packets in %d categories, %d batches, completed in %v",
		client.username, len(packets), len(categoryOrder), totalBatches, elapsed))
}

///////////////

func (ts *Teamserver) TsPresyncExtenders() []interface{} {
	var packets []interface{}
	ts.listener_configs.ForEach(func(key string, value interface{}) bool {
		listenerInfo := value.(extender.ListenerInfo)
		p := CreateSpListenerReg(listenerInfo.Name, listenerInfo.Protocol, listenerInfo.Type, listenerInfo.AX)
		packets = append(packets, p)
		return true
	})

	ts.agent_configs.ForEach(func(key string, value interface{}) bool {
		agentInfo := value.(extender.AgentInfo)
		p := CreateSpAgentReg(agentInfo.Name, agentInfo.AX, agentInfo.Listeners)
		packets = append(packets, p)
		return true
	})

	return packets
}

func (ts *Teamserver) TsPresyncListeners() []interface{} {
	var packets []interface{}
	ts.listeners.ForEach(func(key string, value interface{}) bool {
		listenerData := value.(adaptix.ListenerData)
		p := CreateSpListenerStart(listenerData)
		packets = append(packets, p)
		return true
	})
	return packets
}

func (ts *Teamserver) TsPresyncAgents() []interface{} {
	var (
		packets      []interface{}
		sortedTasks  []adaptix.TaskData
		sortedAgents []*Agent
	)

	ts.agents.ForEach(func(key string, value interface{}) bool {
		agent := value.(*Agent)
		index := sort.Search(len(sortedAgents), func(i int) bool {
			return sortedAgents[i].Data.CreateTime > agent.Data.CreateTime
		})
		sortedAgents = append(sortedAgents[:index], append([]*Agent{agent}, sortedAgents[index:]...)...)
		return true
	})

	ts.agents.DirectLock()
	for _, agent := range sortedAgents {
		if agent != nil {
			/// Agent
			p := CreateSpAgentNew(agent.Data)
			packets = append(packets, p)

			/// Tasks
			agent.CompletedTasks.ForEach(func(key2 string, value2 interface{}) bool {
				taskData := value2.(adaptix.TaskData)
				index := sort.Search(len(sortedTasks), func(i int) bool {
					return sortedTasks[i].StartDate > taskData.StartDate
				})
				sortedTasks = append(sortedTasks[:index], append([]adaptix.TaskData{taskData}, sortedTasks[index:]...)...)
				return true
			})

			/// Consoles - Use DirectAccess for better performance
			agent.OutConsole.DirectAccess(func(item interface{}) {
				packets = append(packets, item)
			})
		}
	}
	ts.agents.DirectUnlock()

	for _, taskData := range sortedTasks {
		t := CreateSpAgentTaskSync(taskData)
		packets = append(packets, t)
	}

	return packets
}

func (ts *Teamserver) TsPresyncPivots() []interface{} {
	var packets []interface{}
	ts.pivots.DirectAccess(func(item interface{}) {
		pivot := item.(*adaptix.PivotData)
		p := CreateSpPivotCreate(*pivot)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncChat() []interface{} {
	var packets []interface{}
	ts.messages.DirectAccess(func(item interface{}) {
		message := item.(adaptix.ChatData)
		p := CreateSpChatMessage(message)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncDownloads() []interface{} {
	var (
		packets         []interface{}
		sortedDownloads []adaptix.DownloadData
	)

	ts.downloads.ForEach(func(key string, value interface{}) bool {
		downloadData := value.(adaptix.DownloadData)
		index := sort.Search(len(sortedDownloads), func(i int) bool {
			return sortedDownloads[i].Date > downloadData.Date
		})
		sortedDownloads = append(sortedDownloads[:index], append([]adaptix.DownloadData{downloadData}, sortedDownloads[index:]...)...)
		return true
	})

	for _, downloadData := range sortedDownloads {
		d1 := CreateSpDownloadCreate(downloadData)
		d2 := CreateSpDownloadUpdate(downloadData)
		packets = append(packets, d1, d2)
	}

	return packets
}

func (ts *Teamserver) TsPresyncScreenshots() []interface{} {
	var sortedScreens []adaptix.ScreenData
	ts.screenshots.ForEach(func(key string, value interface{}) bool {
		screenData := value.(adaptix.ScreenData)
		index := sort.Search(len(sortedScreens), func(i int) bool {
			return sortedScreens[i].Date > screenData.Date
		})
		sortedScreens = append(sortedScreens[:index], append([]adaptix.ScreenData{screenData}, sortedScreens[index:]...)...)
		return true
	})

	var packets []interface{}
	for _, screenData := range sortedScreens {
		t := CreateSpScreenshotCreate(screenData)
		packets = append(packets, t)
	}
	return packets
}

func (ts *Teamserver) TsPresyncCredentials() []interface{} {
	var creds []*adaptix.CredsData
	ts.credentials.DirectAccess(func(item interface{}) {
		c := item.(*adaptix.CredsData)
		creds = append(creds, c)
	})

	p := CreateSpCredentialsAdd(creds)
	var packets []interface{}
	if len(creds) > 0 {
		packets = append(packets, p)
	}
	return packets
}

func (ts *Teamserver) TsPresyncTargets() []interface{} {
	var targets []*adaptix.TargetData
	ts.targets.DirectAccess(func(item interface{}) {
		target := item.(*adaptix.TargetData)
		targets = append(targets, target)
	})

	p := CreateSpTargetsAdd(targets)
	var packets []interface{}
	if len(targets) > 0 {
		packets = append(packets, p)
	}
	return packets
}

func (ts *Teamserver) TsPresyncTunnels() []interface{} {
	var packets []interface{}
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		t := CreateSpTunnelCreate(tunnel.Data)
		packets = append(packets, t)
		return true
	})
	return packets
}

func (ts *Teamserver) TsPresyncEvents() []interface{} {
	var packets []interface{}
	ts.events.DirectAccess(func(item interface{}) {
		packets = append(packets, item)
	})
	return packets
}
