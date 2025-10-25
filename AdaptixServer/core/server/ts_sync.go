package server

import (
	"AdaptixServer/core/extender"
	"bytes"
	"encoding/json"
	"sort"

	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientConnected(username string) bool {
	_, found := ts.clients.Get(username)
	return found
}

func (ts *Teamserver) TsSyncClient(username string, packet interface{}) {
	var buffer bytes.Buffer
	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	err := encoder.Encode(packet)
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
	var buffer bytes.Buffer
	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	err := encoder.Encode(packet)

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

	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	startPacket := CreateSpSyncStart(len(packets), ts.Parameters.Interfaces)
	_ = encoder.Encode(startPacket)
	_ = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()

	for _, p := range packets {
		var pBuffer bytes.Buffer
		pEncoder := json.NewEncoder(&pBuffer)
		pEncoder.SetEscapeHTML(false)
		_ = pEncoder.Encode(p)
		_ = client.socket.WriteMessage(websocket.BinaryMessage, pBuffer.Bytes())
	}

	finishPacket := CreateSpSyncFinish()
	buffer.Reset()
	_ = encoder.Encode(finishPacket)
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
