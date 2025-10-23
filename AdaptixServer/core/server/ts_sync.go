package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/logs"
	"bytes"
	"encoding/json"
	"sort"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientConnected(username string) bool {
	_, found := ts.clients.Get(username)
	return found
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

	// 调试日志
	packetMap, _ := packet.(map[string]interface{})
	if packetMap == nil {
		// 尝试通过反射获取packet类型
		packetMap = make(map[string]interface{})
	}

	ts.clients.ForEach(func(key string, value interface{}) bool {
		client := value.(*Client)
		if client.synced {
			client.lockSocket.Lock()

			// 设置10秒写入超时，防止无限阻塞导致死锁
			deadline := time.Now().Add(10 * time.Second)
			client.socket.SetWriteDeadline(deadline)

			err := client.socket.WriteMessage(websocket.BinaryMessage, data)

			// 重置WriteDeadline（重要！避免影响后续写入）
			client.socket.SetWriteDeadline(time.Time{})

			client.lockSocket.Unlock()

			if err != nil {
				// 只在出错时记录日志，减少日志量
				logs.Error("", "Failed to send %d bytes to client %s: %v", len(data), key, err)
				go ts.TsClientDisconnect(key)
			}
			// 成功时不记录日志，保持简洁
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

	for _, p := range packets {
		var pBuffer bytes.Buffer
		_ = json.NewEncoder(&pBuffer).Encode(p)
		_ = client.socket.WriteMessage(websocket.BinaryMessage, pBuffer.Bytes())
	}

	finishPacket := CreateSpSyncFinish()
	_ = json.NewEncoder(&buffer).Encode(finishPacket)
	_ = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()
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

			/// Consoles
			for value := range agent.OutConsole.Iterator() {
				packets = append(packets, value.Item)
			}
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
	for value := range ts.pivots.Iterator() {
		pivot := value.Item.(*adaptix.PivotData)
		p := CreateSpPivotCreate(*pivot)
		packets = append(packets, p)
	}
	return packets
}

func (ts *Teamserver) TsPresyncChat() []interface{} {
	var packets []interface{}
	for value := range ts.messages.Iterator() {
		message := value.Item.(adaptix.ChatData)
		p := CreateSpChatMessage(message)
		packets = append(packets, p)
	}
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
	for value := range ts.credentials.Iterator() {
		c := value.Item.(*adaptix.CredsData)
		creds = append(creds, c)
	}

	p := CreateSpCredentialsAdd(creds)
	var packets []interface{}
	if len(creds) > 0 {
		packets = append(packets, p)
	}
	return packets
}

func (ts *Teamserver) TsPresyncTargets() []interface{} {
	var targets []*adaptix.TargetData
	for value := range ts.targets.Iterator() {
		target := value.Item.(*adaptix.TargetData)
		targets = append(targets, target)
	}

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
	for value := range ts.events.Iterator() {
		packets = append(packets, value.Item)
	}
	return packets
}
