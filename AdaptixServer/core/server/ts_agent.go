package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"AdaptixServer/core/utils/tformat"
	isvalid "AdaptixServer/core/utils/valid"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsAgentList() (string, error) {
	var agents []adaptix.AgentData
	ts.agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if ok {
			agents = append(agents, agent.GetData())
		}
		return true
	})

	jsonAgents, err := json.Marshal(agents)
	if err != nil {
		return "", err
	}
	return string(jsonAgents), nil
}

func (ts *Teamserver) TsAgentIsExists(agentId string) bool {
	return ts.agents.Contains(agentId)
}

func (ts *Teamserver) TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error) {
	if beat == nil {
		return adaptix.AgentData{}, fmt.Errorf("agent %v does not register", agentId)
	}

	agentName, ok := ts.wm_agent_types[agentCrc]
	if !ok {
		return adaptix.AgentData{}, fmt.Errorf("agent type %v does not exists", agentCrc)
	}
	ok = ts.agents.Contains(agentId)
	if ok {
		return adaptix.AgentData{}, fmt.Errorf("agent %v already exists", agentId)
	}

	agentData, err := ts.Extender.ExAgentCreate(agentName, beat)
	if err != nil {
		return adaptix.AgentData{}, err
	}

	if agentData.Id == "" {
		agentData.Id = agentId
	}
	agentData.Crc = agentCrc
	agentData.Name = agentName
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
		return agentData, fmt.Errorf("listener %v does not exists", listenerName)
	}

	regName := value.(adaptix.ListenerData).RegName
	value, ok = ts.listener_configs.Get(regName)
	if !ok {
		return agentData, fmt.Errorf("listener %v does not register", regName)
	}

	listenerInfo, _ := value.(extender.ListenerInfo)
	if listenerInfo.Type == "internal" {
		agentData.Mark = "Unlink"
	}

	agent := &Agent{
		OutConsole:        safe.NewSlice(),
		HostedTasks:       safe.NewSafeQueue(0x100),
		HostedTunnelTasks: safe.NewSafeQueue(0x100),
		HostedTunnelData:  safe.NewSafeQueue(0x1000),
		RunningTasks:      safe.NewMap(),
		RunningJobs:       safe.NewMap(),
		CompletedTasks:    safe.NewMap(),
		PivotParent:       nil,
		PivotChilds:       safe.NewSlice(),
		Tick:              false,
		Active:            true,
	}
	agent.SetData(agentData)

	ts.agents.Put(agentData.Id, agent)

	packetNew := CreateSpAgentNew(agentData)
	ts.TsSyncAllClients(packetNew)

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.TargetId, _ = ts.TsTargetsCreateAlive(agentData)
	})

	err = ts.DBMS.DbAgentInsert(agent.GetData())
	if err != nil {
		logs.Error("", err.Error())
	}

	ts.TsEventAgent(false, agent.GetData())

	return agent.GetData(), nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, clientName string, hookId string, cmdline string, ui bool, args map[string]any) error {
	if !ts.agent_configs.Contains(agentName) {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, messageData, err := ts.Extender.ExAgentCommand(agentName, agent.GetData(), args)
	if err != nil {
		return err
	}
	taskData.HookId = hookId
	if taskData.Type == TYPE_TASK && ui {
		taskData.Type = TYPE_BROWSER
	}

	ts.TsTaskCreate(agentId, cmdline, clientName, taskData)

	if (taskData.Type != TYPE_BROWSER) && (len(messageData.Message) > 0 || len(messageData.Text) > 0) {
		ts.TsAgentConsoleOutput(agentId, messageData.Status, messageData.Message, messageData.Text, false)
	}
	return nil
}

func (ts *Teamserver) TsAgentProcessData(agentId string, bodyData []byte) error {

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()
	if agentData.Mark == "Inactive" {
		agent.UpdateData(func(d *adaptix.AgentData) {
			d.Mark = ""
		})
		err := ts.DBMS.DbAgentUpdate(agent.GetData())
		if err != nil {
			logs.Error("", err.Error())
		}
	}

	if len(bodyData) > 4 {
		_, err := ts.Extender.ExAgentProcessData(agent.GetData(), bodyData)
		return err
	}

	return nil
}

/// Get Tasks

func (ts *Teamserver) TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return nil, fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()
	tasksCount := agent.HostedTasks.Len()
	tunnelConnectCount := agent.HostedTunnelTasks.Len()
	tunnelTasksCount := agent.HostedTunnelData.Len()
	pivotTasksExists := false
	if agent.PivotChilds.Len() > 0 {
		pivotTasksExists = ts.TsTasksPivotExists(agentData.Id, true)
	}

	if tasksCount > 0 || tunnelConnectCount > 0 || tunnelTasksCount > 0 || pivotTasksExists {

		tasks, err := ts.TsTaskGetAvailableAll(agentData.Id, maxDataSize)
		if err != nil {
			return nil, err
		}

		respData, err := ts.Extender.ExAgentPackData(agentData, tasks)
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

func (ts *Teamserver) TsAgentGetHostedTasks(agentId string, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return nil, fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()
	tasksCount := agent.HostedTasks.Len()
	if tasksCount == 0 && agent.HostedTunnelTasks.Len() == 0 {
		return []byte(""), nil
	}

	tasks, _, err := ts.TsTaskGetAvailableTasks(agentData.Id, maxDataSize)
	if err != nil {
		return nil, err
	}

	respData, err := ts.Extender.ExAgentPackData(agentData, tasks)
	if err != nil {
		return nil, err
	}

	if tasksCount > 0 {
		message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
		ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "", false)
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentGetHostedTasksCount(agentId string, count int, maxDataSize int) ([]byte, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return nil, fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()
	tasksCount := agent.HostedTasks.Len()
	if tasksCount > 0 {

		tasks, _, err := ts.TsTaskGetAvailableTasksCount(agentData.Id, count, maxDataSize)
		if err != nil {
			return nil, err
		}

		respData, err := ts.Extender.ExAgentPackData(agentData, tasks)
		if err != nil {
			return nil, err
		}

		message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
		ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "", false)

		return respData, nil
	}
	return []byte(""), nil
}

//func (ts *Teamserver) TsAgentGetHostedTunnels(agentId string, channelId int, maxDataSize int) ([]byte, error) {
//	value, ok := ts.agents.Get(agentId)
//	if !ok {
//		return nil, fmt.Errorf("agent type %v does not exists", agentId)
//	}
//	agent, _ := value.(*Agent)
//
//	var tasks []adaptix.TaskData
//	tasksSize := 0
//
//	/// TUNNELS QUEUE
//
//	for i := uint(0); i < agent.HostedTunnelData.Len(); i++ {
//		value, ok = agent.HostedTunnelData.Get(i)
//		if ok {
//			taskDataTunnel := value.(adaptix.TaskDataTunnel)
//			if taskDataTunnel.ChannelId == channelId {
//				if tasksSize+len(taskDataTunnel.Data.Data) < maxDataSize {
//					tasks = append(tasks, taskDataTunnel.Data)
//					agent.HostedTunnelData.Delete(i)
//					i--
//					tasksSize += len(taskDataTunnel.Data.Data)
//				} else {
//					break
//				}
//			}
//		} else {
//			break
//		}
//	}
//
//	if len(tasks) > 0 {
//		respData, err := ts.Extender.ExAgentPackData(agent.Data, tasks)
//		if err != nil {
//			return nil, err
//		}
//		return respData, nil
//	}
//
//	return []byte(""), nil
//}

/// Data

func (ts *Teamserver) TsAgentUpdateData(newAgentData adaptix.AgentData) error {
	value, ok := ts.agents.Get(newAgentData.Id)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Sleep = newAgentData.Sleep
		d.Jitter = newAgentData.Jitter
		d.WorkingTime = newAgentData.WorkingTime
		d.KillDate = newAgentData.KillDate
	})

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentTerminate(agentId string, terminateTaskId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}

	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}
	agent.Active = false
	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Mark = "Terminated"
	})

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
		if term.agent.GetData().Id == agentId {
			terminals = append(terminals, term.TerminalId)
		}
		return true
	})
	for _, id := range terminals {
		termId := fmt.Sprintf("%08x", id)
		_ = ts.TsTerminalConnClose(termId, "agent terminated")
	}

	/// Clear HostedTunnelData

	agent.HostedTunnelData.Clear()

	/// Clear HostedTasks
	for {
		item, err := agent.HostedTasks.Pop()
		if err != nil {
			break
		}
		task := item.(adaptix.TaskData)
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

		if task.Type == TYPE_JOB {
			agent.RunningJobs.Delete(task.TaskId)
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

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentConsoleRemove(agentId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}
	agent.OutConsole.CutArray()

	_ = ts.DBMS.DbConsoleDelete(agentId)

	return nil
}

func (ts *Teamserver) TsAgentRemove(agentId string) error {
	value, ok := ts.agents.GetDelete(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}

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

/// Setters

func (ts *Teamserver) TsAgentSetTag(agentId string, tag string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}

	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Tags = tag
	})

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetMark(agentId string, mark string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	agentData := agent.GetData()
	if agentData.Mark == mark || agentData.Mark == "Terminated" {
		return nil
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Mark = mark
		if mark == "Disconnect" {
			d.LastTick = int(time.Now().Unix())
		}
	})

	agentData = agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetColor(agentId string, background string, foreground string, reset bool) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		if reset {
			d.Color = ""
		} else {
			bcolor := ""
			fcolor := ""
			colors := strings.Split(d.Color, "-")
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
			d.Color = bcolor + "-" + fcolor
		}
	})

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetImpersonate(agentId string, impersonated string, elevated bool) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Impersonated = impersonated
		if impersonated != "" && elevated {
			d.Impersonated += " *"
		}
	})

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentSetTick(agentId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()
	if agentData.Async {
		agent.UpdateData(func(d *adaptix.AgentData) {
			d.LastTick = int(time.Now().Unix())
		})
		_ = ts.DBMS.DbAgentTick(agent.GetData())
		agent.Tick = true
	}
	return nil
}

/// Sync

func (ts *Teamserver) TsAgentTickUpdate() {
	for {
		var agentSlice []string
		ts.agents.ForEach(func(key string, value interface{}) bool {
			agent, ok := value.(*Agent)
			if !ok {
				return true
			}
			agentData := agent.GetData()
			if agentData.Async {
				if agent.Tick {
					agent.Tick = false
					agentSlice = append(agentSlice, agentData.Id)
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
