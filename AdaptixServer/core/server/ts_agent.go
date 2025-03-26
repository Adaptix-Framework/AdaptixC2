package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"AdaptixServer/core/utils/tformat"
	isvalid "AdaptixServer/core/utils/valid"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

func (ts *Teamserver) TsAgentIsExists(agentId string) bool {
	return ts.agents.Contains(agentId)
}

func (ts *Teamserver) TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) error {

	if beat == nil {
		return fmt.Errorf("agent %v does not register", agentId)
	}

	agentName, ok := ts.wm_agent_types[agentCrc]
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentCrc)
	}

	_, ok = ts.agents.Get(agentId)
	if ok {
		return fmt.Errorf("agent %v already exists", agentId)
	}

	data, err := ts.Extender.ExAgentCreate(agentName, beat)
	if err != nil {
		return err
	}

	var agentData adaptix.AgentData
	err = json.Unmarshal(data, &agentData)
	if err != nil {
		return err
	}

	agentData.Crc = agentCrc
	agentData.Name = agentName
	agentData.Id = agentId
	agentData.Listener = listenerName
	agentData.ExternalIP = ExternalIP
	agentData.CreateTime = time.Now().Unix()
	agentData.LastTick = int(time.Now().Unix())
	agentData.Async = Async
	agentData.Tags = ""
	agentData.Mark = ""
	agentData.Color = ""

	value, _ := ts.listeners.Get(listenerName)
	if value.(adaptix.ListenerData).Type == "internal" {
		agentData.Mark = "Unlink"
	}

	agent := &Agent{
		Data:           agentData,
		OutConsole:     safe.NewSlice(),
		TunnelQueue:    safe.NewSlice(),
		TasksQueue:     safe.NewSlice(),
		RunningTasks:   safe.NewMap(),
		CompletedTasks: safe.NewMap(),
		PivotParent:    nil,
		PivotChilds:    safe.NewSlice(),
		Tick:           false,
		Active:         true,
	}

	ts.agents.Put(agentData.Id, agent)

	err = ts.DBMS.DbAgentInsert(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentNew(agentData)
	ts.TsSyncAllClients(packetNew)

	message := fmt.Sprintf("New '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
	packet2 := CreateSpEvent(EVENT_AGENT_NEW, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

	return nil
}

func (ts *Teamserver) TsAgentProcessData(agentId string, bodyData []byte) error {

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, _ := value.(*Agent)

	/// AGENT TICK

	if agent.Data.Async {
		agent.Data.LastTick = int(time.Now().Unix())
		_ = ts.DBMS.DbAgentTick(agent.Data)
		agent.Tick = true
	}
	if agent.Data.Mark == "Inactive" {
		agent.Data.Mark = ""
		err := ts.DBMS.DbAgentUpdate(agent.Data)
		if err != nil {
			logs.Error("", err.Error())
		}
	}

	var agentBuffer bytes.Buffer
	_ = json.NewEncoder(&agentBuffer).Encode(agent.Data)
	if len(bodyData) > 4 {
		_, err := ts.Extender.ExAgentProcessData(agent.Data.Name, agentBuffer.Bytes(), bodyData)
		return err
	}

	return nil
}

func (ts *Teamserver) TsAgentGetHostedTasks(agentId string, maxDataSize int) ([]byte, error) {
	var (
		respData    []byte
		err         error
		agentBuffer bytes.Buffer
	)

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, _ := value.(*Agent)

	_ = json.NewEncoder(&agentBuffer).Encode(agent.Data)

	tasksCount := agent.TasksQueue.Len()
	tunnelTasksCount := agent.TunnelQueue.Len()
	pivotTasksExists := false
	if agent.PivotChilds.Len() > 0 {
		pivotTasksExists = ts.TsTasksPivotExists(agent.Data.Id, true)
	}

	if tasksCount > 0 || tunnelTasksCount > 0 || pivotTasksExists {
		respData, err = ts.Extender.ExAgentPackData(agent.Data.Name, agentBuffer.Bytes(), maxDataSize)
		if err != nil {
			return nil, err
		}

		if tasksCount > 0 {
			message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
			ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "", false)
		}
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, clientName string, cmdline string, args map[string]any) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
	)

	if ts.agent_configs.Contains(agentName) {

		value, ok := ts.agents.Get(agentId)
		if ok {

			agent, _ = value.(*Agent)
			if agent.Active == false {
				return fmt.Errorf("agent '%v' not active", agentId)
			}

			_ = json.NewEncoder(&agentObject).Encode(agent.Data)

			err = ts.Extender.ExAgentCommand(clientName, cmdline, agentName, agentObject.Bytes(), args)
			if err != nil {
				return err
			}

		} else {
			return fmt.Errorf("agent '%v' does not exist", agentId)
		}
	} else {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	return nil
}

/// Data

func (ts *Teamserver) TsAgentUpdateData(newAgentObject []byte) error {
	var (
		agent        *Agent
		err          error
		newAgentData adaptix.AgentData
	)

	err = json.Unmarshal(newAgentObject, &newAgentData)
	if err != nil {
		return err
	}

	value, ok := ts.agents.Get(newAgentData.Id)
	if !ok {
		return errors.New("agent does not exist")
	}

	agent, _ = value.(*Agent)
	agent.Data.Sleep = newAgentData.Sleep
	agent.Data.Jitter = newAgentData.Jitter

	err = ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentImpersonate(agentId string, impersonated string, elevated bool) error {
	var (
		agent *Agent
		err   error
	)

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, _ = value.(*Agent)

	agent.Data.Impersonated = impersonated
	if impersonated != "" && elevated {
		agent.Data.Impersonated += " *"
	}

	err = ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentTerminate(agentId string, terminateTaskId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}

	agent, _ := value.(*Agent)
	agent.Active = false
	agent.Data.Mark = "Terminated"

	/// Clear Downloads

	var downloads []string
	ts.downloads.ForEach(func(key string, value interface{}) bool {
		downloadData := value.(adaptix.DownloadData)
		if downloadData.AgentId == agentId && downloadData.State != DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, DOWNLOAD_STATE_CANCELED)
	}

	/// Clear Tunnels

	var tunnels []string
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		if tunnel.Data.AgentId == agentId {
			tunnels = append(tunnels, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnels {
		_ = ts.TsTunnelStop(id)
	}

	/// Clear TunnelQueue

	_ = agent.TunnelQueue.CutArray()

	/// Clear TasksQueue

	tasksQueue := agent.TasksQueue.CutArray()
	for _, value = range tasksQueue {
		task := value.(adaptix.TaskData)
		packet := CreateSpAgentTaskRemove(task)
		ts.TsSyncAllClients(packet)
	}

	/// Clear TasksRunning

	tasksRunning := agent.RunningTasks.CutMap()
	for _, value = range tasksRunning {
		task := value.(adaptix.TaskData)
		if task.TaskId == terminateTaskId {
			agent.RunningTasks.Put(task.TaskId, task)
		} else {
			packet := CreateSpAgentTaskRemove(task)
			ts.TsSyncAllClients(packet)
		}
	}

	/// Clear Pivots

	if agent.PivotParent != nil {
		_ = ts.TsPivotDelete(agent.PivotParent.PivotId)
	}

	var pivots []string
	for value := range agent.PivotChilds.Iterator() {
		pivotId := value.Item.(*adaptix.PivotData).PivotId
		pivots = append(pivots, pivotId)
	}
	for _, pivotId := range pivots {
		_ = ts.TsPivotDelete(pivotId)
	}

	/// Update

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentRemove(agentId string) error {
	value, ok := ts.agents.GetDelete(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent := value.(*Agent)

	/// Clear Downloads

	var downloads []string
	ts.downloads.ForEach(func(key string, value interface{}) bool {
		downloadData := value.(adaptix.DownloadData)
		if downloadData.AgentId == agentId && downloadData.State != DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, DOWNLOAD_STATE_CANCELED)
	}

	/// Clear Tunnels

	var tunnels []string
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		if tunnel.Data.AgentId == agentId {
			tunnels = append(tunnels, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnels {
		_ = ts.TsTunnelStop(id)
	}

	if agent.PivotParent != nil {
		_ = ts.TsPivotDelete(agent.PivotParent.PivotId)
	}

	var pivots []string
	for value := range agent.PivotChilds.Iterator() {
		pivotId := value.Item.(*adaptix.PivotData).PivotId
		pivots = append(pivots, pivotId)
	}
	for _, pivotId := range pivots {
		_ = ts.TsPivotDelete(pivotId)
	}

	err := ts.DBMS.DbAgentDelete(agentId)
	if err != nil {
		logs.Error("", err.Error())
	} else {
		_ = ts.DBMS.DbTaskDelete("", agentId)
		_ = ts.DBMS.DbConsoleDelete(agentId)
	}

	packet := CreateSpAgentRemove(agentId)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsAgentSetTag(agentId string, tag string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}

	agent, _ := value.(*Agent)
	agent.Data.Tags = tag

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetMark(agentId string, mark string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, _ := value.(*Agent)

	if agent.Data.Mark == mark || agent.Data.Mark == "Terminated" {
		return nil
	}

	agent.Data.Mark = mark

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetColor(agentId string, background string, foreground string, reset bool) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, _ := value.(*Agent)

	if reset {
		agent.Data.Color = ""
	} else {
		bcolor := ""
		fcolor := ""
		colors := strings.Split(agent.Data.Color, "-")
		if len(colors) == 2 {
			bcolor = colors[0]
			fcolor = colors[1]
		}
		if isvalid.ValidColorRGB(background) {
			bcolor = background
		}
		if isvalid.ValidColorRGB(foreground) {
			fcolor = foreground
		}
		agent.Data.Color = bcolor + "-" + fcolor
	}

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

/// Sync

func (ts *Teamserver) TsAgentTickUpdate() {
	for {
		var agentSlice []string
		ts.agents.ForEach(func(key string, value interface{}) bool {
			agent := value.(*Agent)
			if agent.Data.Async {
				if agent.Tick {
					agent.Tick = false
					agentSlice = append(agentSlice, agent.Data.Id)
				}
			}
			return true
		})

		if len(agentSlice) > 0 {
			packetTick := CreateSpAgentTick(agentSlice)
			ts.TsSyncAllClients(packetTick)
		}

		time.Sleep(800 * time.Millisecond)
	}
}

func (ts *Teamserver) TsAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	return ts.Extender.ExAgentGenerate(agentName, config, listenerWM, listenerProfile)
}

/// Console

func (ts *Teamserver) TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncAllClients(packet)

	if store {
		_ = ts.DBMS.DbConsoleInsert(agentId, packet)
	}
}

func (ts *Teamserver) TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncClient(client, packet)
}
