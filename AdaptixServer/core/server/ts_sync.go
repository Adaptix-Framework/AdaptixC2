package server

import (
	"AdaptixServer/core/extender"
	"bytes"
	"encoding/json"
	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
	"sort"
)

func (ts *Teamserver) TsSyncClient(username string, packet interface{}) {
	var (
		buffer   bytes.Buffer
		err      error
		clientWS *websocket.Conn
	)

	err = json.NewEncoder(&buffer).Encode(packet)
	if err != nil {
		return
	}

	value, found := ts.clients.Get(username)
	if found {
		client := value.(*Client)
		clientWS = client.socket
		err = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
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

func (ts *Teamserver) TsSyncStored(clientWS *websocket.Conn) {
	var (
		buffer  bytes.Buffer
		packets []interface{}
	)

	packets = append(packets, ts.TsPresyncExtenders()...)
	packets = append(packets, ts.TsPresyncListeners()...)
	packets = append(packets, ts.TsPresyncAgents()...)
	packets = append(packets, ts.TsPresyncDownloads()...)
	packets = append(packets, ts.TsPresyncScreenshots()...)
	packets = append(packets, ts.TsPresyncTunnels()...)
	packets = append(packets, ts.TsPresyncEvents()...)
	packets = append(packets, ts.TsPresyncPivots()...)
	packets = append(packets, ts.TsPresyncCredentials()...)

	startPacket := CreateSpSyncStart(len(packets))
	_ = json.NewEncoder(&buffer).Encode(startPacket)
	_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()

	for _, p := range packets {
		var pBuffer bytes.Buffer
		_ = json.NewEncoder(&pBuffer).Encode(p)
		_ = clientWS.WriteMessage(websocket.BinaryMessage, pBuffer.Bytes())
	}

	finishPacket := CreateSpSyncFinish()
	_ = json.NewEncoder(&buffer).Encode(finishPacket)
	_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	buffer.Reset()
}

///////////////

func (ts *Teamserver) TsPresyncExtenders() []interface{} {
	var packets []interface{}
	ts.listener_configs.ForEach(func(key string, value interface{}) bool {
		listenerInfo := value.(extender.ListenerInfo)
		p := CreateSpListenerReg(key, listenerInfo.AX)
		packets = append(packets, p)
		return true
	})

	ts.agent_configs.ForEach(func(key string, value interface{}) bool {
		agentInfo := value.(extender.AgentInfo)
		p := CreateSpAgentReg(agentInfo.Name, agentInfo.Watermark, agentInfo.AX, agentInfo.Listeners)
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

func (ts *Teamserver) TsPresyncDownloads() []interface{} {
	var packets []interface{}
	ts.downloads.ForEach(func(key string, value interface{}) bool {
		downloadData := value.(adaptix.DownloadData)
		d1 := CreateSpDownloadCreate(downloadData)
		d2 := CreateSpDownloadUpdate(downloadData)
		packets = append(packets, d1, d2)
		return true
	})
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
	var packets []interface{}
	for value := range ts.credentials.Iterator() {
		creds := value.Item.(*adaptix.CredsData)
		p := CreateSpCredentialsAdd(*creds)
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
