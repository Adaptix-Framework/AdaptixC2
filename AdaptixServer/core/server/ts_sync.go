package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/extender"
	"bytes"
	"encoding/json"
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

	ts.clients.ForEach(func(key string, value interface{}) {
		client := value.(*Client)
		if client.synced {
			err = json.NewEncoder(&buffer).Encode(packet)
			if err != nil {
				return
			}

			clientWS := client.socket
			_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		} else {
			client.tmp_store.Put(packet)
		}
	})
}

func (ts *Teamserver) TsSyncStored(clientWS *websocket.Conn) {
	var (
		buffer  bytes.Buffer
		packets []interface{}
	)

	packets = append(packets, ts.TsPresyncExtenders()...)
	packets = append(packets, ts.TsPresyncListeners()...)
	packets = append(packets, ts.TsPresyncAgentsAndTasks()...)
	packets = append(packets, ts.TsPresyncDownloads()...)
	packets = append(packets, ts.TsPresyncEvents()...)

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
	ts.listener_configs.ForEach(func(key string, value interface{}) {
		listenerInfo := value.(extender.ListenerInfo)
		p := CreateSpListenerReg(key, listenerInfo.ListenerUI)
		packets = append(packets, p)
	})
	ts.agent_configs.ForEach(func(key string, value interface{}) {
		agentInfo := value.(extender.AgentInfo)
		p := CreateSpAgentReg(key, agentInfo.ListenerName, agentInfo.AgentUI, agentInfo.AgentCmd)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncListeners() []interface{} {
	var packets []interface{}
	ts.listeners.ForEach(func(key string, value interface{}) {
		listenerData := value.(adaptix.ListenerData)
		p := CreateSpListenerStart(listenerData)
		packets = append(packets, p)
	})
	return packets
}

func (ts *Teamserver) TsPresyncAgentsAndTasks() []interface{} {
	var packets []interface{}
	ts.agents.ForEach(func(key string, value interface{}) {
		agent := value.(*Agent)
		p := CreateSpAgentNew(agent.Data)
		packets = append(packets, p)

		var sortedTasks []adaptix.TaskData

		agent.ClosedTasks.ForEach(func(key2 string, value2 interface{}) {
			taskData := value2.(adaptix.TaskData)
			index := sort.Search(len(sortedTasks), func(i int) bool {
				return sortedTasks[i].StartDate > taskData.StartDate
			})
			sortedTasks = append(sortedTasks[:index], append([]adaptix.TaskData{taskData}, sortedTasks[index:]...)...)
		})

		for _, taskData := range sortedTasks {
			t1 := CreateSpAgentTaskCreate(taskData)
			t2 := CreateSpAgentTaskUpdate(taskData)
			packets = append(packets, t1, t2)
		}

	})
	return packets
}

func (ts *Teamserver) TsPresyncDownloads() []interface{} {
	var packets []interface{}
	ts.downloads.ForEach(func(key string, value interface{}) {
		downloadData := value.(adaptix.DownloadData)
		d1 := CreateSpDownloadCreate(downloadData)
		d2 := CreateSpDownloadUpdate(downloadData)
		packets = append(packets, d1, d2)
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
