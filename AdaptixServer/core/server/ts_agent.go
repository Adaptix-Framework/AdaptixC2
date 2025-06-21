package server

import (
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"AdaptixServer/core/utils/tformat"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
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
	ok = ts.agents.Contains(agentId)
	if ok {
		return fmt.Errorf("agent %v already exists", agentId)
	}

	agentData, err := ts.Extender.ExAgentCreate(agentName, beat)
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

	value, ok := ts.listeners.Get(listenerName)
	if !ok {
		return fmt.Errorf("listener %v does not exists", listenerName)
	}

	lType := strings.Split(value.(adaptix.ListenerData).Type, "/")[0]
	if lType == "internal" {
		agentData.Mark = "Unlink"
	}

	agent := &Agent{
		Data:               agentData,
		OutConsole:         safe.NewSlice(),
		TasksQueue:         safe.NewSlice(),
		TunnelConnectTasks: safe.NewSlice(),
		TunnelQueue:        safe.NewSlice(),
		RunningTasks:       safe.NewMap(),
		CompletedTasks:     safe.NewMap(),
		PivotParent:        nil,
		PivotChilds:        safe.NewSlice(),
		Tick:               false,
		Active:             true,
	}

	ts.agents.Put(agentData.Id, agent)

	err = ts.DBMS.DbAgentInsert(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentNew(agentData)
	ts.TsSyncAllClients(packetNew)

	message := ""
	if len(agentData.Domain) == 0 || strings.ToLower(agentData.Computer) == strings.ToLower(agentData.Domain) {
		message = fmt.Sprintf("New '%v' (%v) executed on '%v @ %v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.InternalIP)
	} else {
		message = fmt.Sprintf("New '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
	}
	packet2 := CreateSpEvent(EVENT_AGENT_NEW, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

	return nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, clientName string, cmdline string, args map[string]any) error {
	if !ts.agent_configs.Contains(agentName) {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent, _ := value.(*Agent)

	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	return ts.Extender.ExAgentCommand(clientName, cmdline, agentName, agent.Data, args)
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

	if len(bodyData) > 4 {
		_, err := ts.Extender.ExAgentProcessData(agent.Data, bodyData)
		return err
	}

	return nil
}

/// Get Tasks

func (ts *Teamserver) TsAgentGetHostedTasksAll(agentId string, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, _ := value.(*Agent)

	tasksCount := agent.TasksQueue.Len()
	tunnelConnectCount := agent.TunnelConnectTasks.Len()
	tunnelTasksCount := agent.TunnelQueue.Len()
	pivotTasksExists := false
	if agent.PivotChilds.Len() > 0 {
		pivotTasksExists = ts.TsTasksPivotExists(agent.Data.Id, true)
	}

	if tasksCount > 0 || tunnelConnectCount > 0 || tunnelTasksCount > 0 || pivotTasksExists {

		tasks, err := ts.TsTaskGetAvailableAll(agent.Data.Id, maxDataSize)
		if err != nil {
			return nil, err
		}

		respData, err := ts.Extender.ExAgentPackData(agent.Data, tasks)
		if err != nil {
			return nil, err
		}

		if tasksCount > 0 {
			message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
			ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "", false)
		}
		return respData, nil
	}

	return []byte(""), nil
}

func (ts *Teamserver) TsAgentGetHostedTasksOnly(agentId string, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, _ := value.(*Agent)

	tasksCount := agent.TasksQueue.Len()
	if tasksCount == 0 && agent.TunnelConnectTasks.Len() == 0 {
		return []byte(""), nil
	}

	tasks, _, err := ts.TsTaskGetAvailableTasks(agent.Data.Id, maxDataSize)
	if err != nil {
		return nil, err
	}

	respData, err := ts.Extender.ExAgentPackData(agent.Data, tasks)
	if err != nil {
		return nil, err
	}

	if tasksCount > 0 {
		message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
		ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "", false)
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentGetHostedTasksTunnels(agentId string, channelId int, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, _ := value.(*Agent)

	var tasks []adaptix.TaskData
	tasksSize := 0

	/// TUNNELS QUEUE

	for i := uint(0); i < agent.TunnelQueue.Len(); i++ {
		value, ok = agent.TunnelQueue.Get(i)
		if ok {
			taskDataTunnel := value.(adaptix.TaskDataTunnel)
			if taskDataTunnel.ChannelId == channelId {
				if tasksSize+len(taskDataTunnel.Data.Data) < maxDataSize {
					tasks = append(tasks, taskDataTunnel.Data)
					agent.TunnelQueue.Delete(i)
					i--
					tasksSize += len(taskDataTunnel.Data.Data)
				} else {
					break
				}
			}
		} else {
			break
		}
	}

	if len(tasks) > 0 {
		respData, err := ts.Extender.ExAgentPackData(agent.Data, tasks)
		if err != nil {
			return nil, err
		}
		return respData, nil
	}

	return []byte(""), nil
}

/// Data

func (ts *Teamserver) TsAgentUpdateData(newAgentData adaptix.AgentData) error {
	value, ok := ts.agents.Get(newAgentData.Id)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, _ := value.(*Agent)
	agent.Data.Sleep = newAgentData.Sleep
	agent.Data.Jitter = newAgentData.Jitter
	agent.Data.WorkingTime = newAgentData.WorkingTime
	agent.Data.KillDate = newAgentData.KillDate

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentImpersonate(agentId string, impersonated string, elevated bool) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, _ := value.(*Agent)

	agent.Data.Impersonated = impersonated
	if impersonated != "" && elevated {
		agent.Data.Impersonated += " *"
	}

	err := ts.DBMS.DbAgentUpdate(agent.Data)
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

	/// Clear Terminals

	var terminals []int
	ts.terminals.ForEach(func(key string, value interface{}) bool {
		term := value.(*Terminal)
		if term.agent.Data.Id == agentId {
			terminals = append(terminals, term.TerminalId)
		}
		return true
	})
	for _, id := range terminals {
		termId := fmt.Sprintf("%08x", id)
		_ = ts.TsTerminalConnClose(termId, "agent terminated")
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
		if task.TaskId == terminateTaskId && task.Sync {
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

func (ts *Teamserver) TsAgentConsoleRemove(agentId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent := value.(*Agent)
	agent.OutConsole.CutArray()

	_ = ts.DBMS.DbConsoleDelete(agentId)

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

	if mark == "Disconnect" {
		agent.Data.LastTick = int(time.Now().Unix())
	}

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

func (ts *Teamserver) TsAgentGenerate(agentName string, config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	return ts.Extender.ExAgentGenerate(agentName, config, operatingSystem, listenerWM, listenerProfile)
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
