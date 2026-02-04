package server

import (
	"AdaptixServer/core/eventing"
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
	ts.Agents.ForEach(func(key string, value interface{}) bool {
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
	return ts.Agents.Contains(agentId)
}

func (ts *Teamserver) TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error) {
	if beat == nil {
		return adaptix.AgentData{}, fmt.Errorf("agent %v does not register", agentId)
	}

	agentName, ok := ts.wm_agent_types[agentCrc]
	if !ok {
		return adaptix.AgentData{}, fmt.Errorf("agent type %v does not exists", agentCrc)
	}
	ok = ts.Agents.Contains(agentId)
	if ok {
		return adaptix.AgentData{}, fmt.Errorf("agent %v already exists", agentId)
	}

	agentData, handler, err := ts.Extender.ExAgentCreate(agentName, beat)
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

	agent := &Agent{
		Extender:          handler,
		OutConsole:        safe.NewSlice(),
		HostedTasks:       safe.NewSafeQueue(0x100),
		HostedTunnelTasks: safe.NewSafeQueue(0x1000),
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

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentNew{Agent: agentData, Restore: false}
	if !ts.EventManager.Emit(eventing.EventAgentNew, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return adaptix.AgentData{}, preEvent.Error
		}
		return adaptix.AgentData{}, fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	ts.Agents.Put(agentData.Id, agent)

	packetNew := CreateSpAgentNew(agentData)
	ts.TsSyncAllClientsWithCategory(packetNew, SyncCategoryAgents)

	agent.UpdateData(func(d *adaptix.AgentData) {
		d.TargetId, _ = ts.TsTargetsCreateAlive(agentData)
	})

	err = ts.DBMS.DbAgentInsert(agent.GetData())
	if err != nil {
		logs.Error("", err.Error())
	}

	ts.TsNotifyAgent(false, agent.GetData())

	// --- POST HOOK ---
	postEvent := &eventing.EventDataAgentNew{Agent: agent.GetData(), Restore: false}
	ts.EventManager.EmitAsync(eventing.EventAgentNew, postEvent)
	// -----------------

	return agent.GetData(), nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, clientName string, hookId string, handlerId string, cmdline string, ui bool, args map[string]any) error {
	if !ts.agent_configs.Contains(agentName) {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	agent, err := ts.getAgent(agentId)
	if err != nil {
		return err
	}
	if !agent.Active {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, messageData, err := agent.Command(args)
	if err != nil {
		return err
	}
	if taskData.Type == adaptix.TASK_TYPE_LOCAL {
		if taskData.Message != "" || taskData.ClearText != "" {
			ts.TsAgentConsoleLocalCommand(agentId, clientName, cmdline, taskData.Message, taskData.ClearText)
		}
	} else {
		taskData.HookId = hookId
		taskData.HandlerId = handlerId
		if taskData.Type == adaptix.TASK_TYPE_TASK && ui {
			taskData.Type = adaptix.TASK_TYPE_BROWSER
		}

		ts.TsTaskCreate(agentId, cmdline, clientName, taskData)

		if (taskData.Type != adaptix.TASK_TYPE_BROWSER) && (len(messageData.Message) > 0 || len(messageData.Text) > 0) {
			ts.TsAgentConsoleOutput(agentId, messageData.Status, messageData.Message, messageData.Text, false)
		}

	}

	return nil
}

func (ts *Teamserver) TsAgentProcessData(agentId string, bodyData []byte) error {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return err
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

		updatedAgentData := agent.GetData()

		//packetNew := CreateSpAgentNew(updatedAgentData)
		//ts.TsSyncAgentActivated(packetNew)

		// --- POST HOOK ---
		postEvent := &eventing.EventDataAgentActivate{Agent: updatedAgentData}
		ts.EventManager.EmitAsync(eventing.EventAgentActivate, postEvent)
		// -----------------
	}

	if len(bodyData) > 4 {
		return agent.ProcessData(bodyData)
	}

	return nil
}

/// Get Tasks

func (ts *Teamserver) TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, err
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

		respData, err := agent.PackData(tasks)
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
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, err
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

	respData, err := agent.PackData(tasks)
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
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, err
	}

	agentData := agent.GetData()
	tasksCount := agent.HostedTasks.Len()
	if tasksCount > 0 {

		tasks, _, err := ts.TsTaskGetAvailableTasksCount(agentData.Id, count, maxDataSize)
		if err != nil {
			return nil, err
		}

		respData, err := agent.PackData(tasks)
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
//	value, ok := ts.Agents.Get(agentId)
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

type AgentUpdateFields struct {
	InternalIP   *string
	ExternalIP   *string
	GmtOffset    *int
	ACP          *int
	OemCP        *int
	Pid          *string
	Tid          *string
	Arch         *string
	Elevated     *bool
	Process      *string
	Os           *int
	OsDesc       *string
	Domain       *string
	Computer     *string
	Username     *string
	Impersonated *string
}

func (ts *Teamserver) TsAgentUpdateData(newAgentData adaptix.AgentData) error {
	value, ok := ts.Agents.Get(newAgentData.Id)
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
		d.CustomData = NewAgentMessage.CustomData
	})

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncStateWithCategory(packetNew, "agent:"+agentData.Id, SyncCategoryAgents)

	return nil
}

func (ts *Teamserver) TsAgentUpdateDataPartial(agentId string, updateData interface{}) error {
	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}

	syncPacket := SyncPackerAgentUpdate{
		SpType: TYPE_AGENT_UPDATE,
		Id:     agentId,
	}

	ts.applyAgentUpdate(agent, updateData, &syncPacket)

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		logs.Error("", err.Error())
	}

	ts.TsSyncAllClients(syncPacket)

	return nil
}

func (ts *Teamserver) applyAgentUpdate(agent *Agent, updateData interface{}, syncPacket *SyncPackerAgentUpdate) {
	type fieldAccessor struct {
		InternalIP   *string `json:"internal_ip,omitempty"`
		ExternalIP   *string `json:"external_ip,omitempty"`
		GmtOffset    *int    `json:"gmt_offset,omitempty"`
		ACP          *int    `json:"acp,omitempty"`
		OemCP        *int    `json:"oemcp,omitempty"`
		Pid          *string `json:"pid,omitempty"`
		Tid          *string `json:"tid,omitempty"`
		Arch         *string `json:"arch,omitempty"`
		Elevated     *bool   `json:"elevated,omitempty"`
		Process      *string `json:"process,omitempty"`
		Os           *int    `json:"os,omitempty"`
		OsDesc       *string `json:"os_desc,omitempty"`
		Domain       *string `json:"domain,omitempty"`
		Computer     *string `json:"computer,omitempty"`
		Username     *string `json:"username,omitempty"`
		Impersonated *string `json:"impersonated,omitempty"`
		Tags         *string `json:"tags,omitempty"`
		Mark         *string `json:"mark,omitempty"`
		Color        *string `json:"color,omitempty"`
		Listener     *string `json:"listener,omitempty"`
	}

	jsonBytes, err := json.Marshal(updateData)
	if err != nil {
		return
	}

	var fields fieldAccessor
	if err := json.Unmarshal(jsonBytes, &fields); err != nil {
		return
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		if fields.InternalIP != nil {
			d.InternalIP = *fields.InternalIP
			syncPacket.InternalIP = fields.InternalIP
		}
		if fields.ExternalIP != nil {
			d.ExternalIP = *fields.ExternalIP
			syncPacket.ExternalIP = fields.ExternalIP
		}
		if fields.GmtOffset != nil {
			d.GmtOffset = *fields.GmtOffset
			syncPacket.GmtOffset = fields.GmtOffset
		}
		if fields.ACP != nil {
			d.ACP = *fields.ACP
			syncPacket.ACP = fields.ACP
		}
		if fields.OemCP != nil {
			d.OemCP = *fields.OemCP
			syncPacket.OemCP = fields.OemCP
		}
		if fields.Pid != nil {
			d.Pid = *fields.Pid
			syncPacket.Pid = fields.Pid
		}
		if fields.Tid != nil {
			d.Tid = *fields.Tid
			syncPacket.Tid = fields.Tid
		}
		if fields.Arch != nil {
			d.Arch = *fields.Arch
			syncPacket.Arch = fields.Arch
		}
		if fields.Elevated != nil {
			d.Elevated = *fields.Elevated
			syncPacket.Elevated = fields.Elevated
		}
		if fields.Process != nil {
			d.Process = *fields.Process
			syncPacket.Process = fields.Process
		}
		if fields.Os != nil {
			d.Os = *fields.Os
			syncPacket.Os = fields.Os
		}
		if fields.OsDesc != nil {
			d.OsDesc = *fields.OsDesc
			syncPacket.OsDesc = fields.OsDesc
		}
		if fields.Domain != nil {
			d.Domain = *fields.Domain
			syncPacket.Domain = fields.Domain
		}
		if fields.Computer != nil {
			d.Computer = *fields.Computer
			syncPacket.Computer = fields.Computer
		}
		if fields.Username != nil {
			d.Username = *fields.Username
			syncPacket.Username = fields.Username
		}
		if fields.Impersonated != nil {
			d.Impersonated = *fields.Impersonated
			syncPacket.Impersonated = fields.Impersonated
		}
		if fields.Tags != nil {
			d.Tags = *fields.Tags
			syncPacket.Tags = fields.Tags
		}
		if fields.Listener != nil {
			d.Listener = *fields.Listener
			syncPacket.Listener = fields.Listener
		}
		if fields.Mark != nil {
			if d.Mark != "Terminated" && d.Mark != *fields.Mark {
				d.Mark = *fields.Mark
				syncPacket.Mark = fields.Mark
				if *fields.Mark == "Disconnect" {
					d.LastTick = int(time.Now().Unix())
				}
			}
		}
		if fields.Color != nil {

			if *fields.Color != "" {
				bcolor := ""
				fcolor := ""
				colors := strings.Split(d.Color, "-")
				if len(colors) == 2 {
					bcolor = colors[0]
					fcolor = colors[1]
				}

				newcolors := strings.Split(*fields.Color, "-")
				if len(newcolors) == 2 {
					if isvalid.ValidColorRGB(newcolors[0]) {
						bcolor = newcolors[0]
					}
					if isvalid.ValidColorRGB(newcolors[1]) {
						fcolor = newcolors[1]
					}
				}
				*fields.Color = bcolor + "-" + fcolor
			}

			d.Color = *fields.Color
			syncPacket.Color = fields.Color
		}
	})
}

func (ts *Teamserver) TsAgentTerminate(agentId string, terminateTaskId string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentTerminate{AgentId: agentId, TaskId: terminateTaskId}
	if !ts.EventManager.Emit(eventing.EventAgentTerminate, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	value, ok := ts.Agents.Get(agentId)
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
		if downloadData.AgentId == agentId && downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, adaptix.DOWNLOAD_STATE_CANCELED)
	}

	/// Clear Tunnels

	var tunnelIds []string
	ts.TunnelManager.ForEachTunnel(func(key string, tunnel *Tunnel) bool {
		if tunnel.Data.AgentId == agentId {
			tunnelIds = append(tunnelIds, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnelIds {
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

		if task.Type == adaptix.TASK_TYPE_JOB {
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
	ts.TsSyncStateWithCategory(packetNew, "agent:"+agentId, SyncCategoryAgents)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataAgentTerminate{AgentId: agentId, TaskId: terminateTaskId}
	ts.EventManager.EmitAsync(eventing.EventAgentTerminate, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsAgentConsoleRemove(agentId string) error {
	value, ok := ts.Agents.Get(agentId)
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
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentRemove{}
	if value, ok := ts.Agents.Get(agentId); ok {
		if agent, ok := value.(*Agent); ok {
			preEvent.Agent = agent.GetData()
		}
	}
	if !ts.EventManager.Emit(eventing.EventAgentRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	value, ok := ts.Agents.GetDelete(agentId)
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
		if downloadData.AgentId == agentId && downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, adaptix.DOWNLOAD_STATE_CANCELED)
	}

	/// Clear Tunnels

	var tunnelIds2 []string
	ts.TunnelManager.ForEachTunnel(func(key string, tunnel *Tunnel) bool {
		if tunnel.Data.AgentId == agentId {
			tunnelIds2 = append(tunnelIds2, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnelIds2 {
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
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryAgents)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataAgentRemove{Agent: agent.GetData()}
	ts.EventManager.EmitAsync(eventing.EventAgentRemove, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsAgentSetTick(agentId string, listenerName string) error {
	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return fmt.Errorf("invalid agent type for '%v'", agentId)
	}

	agentData := agent.GetData()

	listenerChanged := (listenerName != "") && (agentData.Listener != listenerName)

	if agentData.Async {
		if listenerChanged {
			agent.UpdateData(func(d *adaptix.AgentData) {
				d.LastTick = int(time.Now().Unix())
				d.Listener = listenerName
			})
			updatedAgentData := agent.GetData()
			packet := CreateSpAgentUpdate(updatedAgentData)
			ts.TsSyncStateWithCategory(packet, "agent:"+agentId, SyncCategoryAgents)
			_ = ts.DBMS.DbAgentUpdate(updatedAgentData)
		} else {
			agent.UpdateData(func(d *adaptix.AgentData) {
				d.LastTick = int(time.Now().Unix())
			})
			_ = ts.DBMS.DbAgentTick(agent.GetData())
		}
		agent.Tick = true
	} else if listenerChanged {
		agent.UpdateData(func(d *adaptix.AgentData) {
			d.Listener = listenerName
		})
		updatedAgentData := agent.GetData()
		packet := CreateSpAgentUpdate(updatedAgentData)
		ts.TsSyncStateWithCategory(packet, "agent:"+agentId, SyncCategoryAgents)
		_ = ts.DBMS.DbAgentUpdate(updatedAgentData)
	}
	return nil
}

/// Sync

func (ts *Teamserver) TsAgentTickUpdate() {
	for {
		var agentSlice []string
		ts.Agents.ForEach(func(key string, value interface{}) bool {
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
			ts.TsSyncStateWithCategory(packetTick, "tick", SyncCategoryAgents)
		}

		time.Sleep(800 * time.Millisecond)
	}
}

/// Console

func (ts *Teamserver) TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncConsole(packet, "")

	if store {
		_ = ts.DBMS.DbConsoleInsert(agentId, packet)
	}
}

func (ts *Teamserver) TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncConsole(packet, client)
}

func (ts *Teamserver) TsAgentConsoleErrorCommand(agentId string, client string, cmdline string, message string, HookId string, HandlerId string) {
	packet := CreateSpAgentErrorCommand(agentId, cmdline, message, HookId, HandlerId)
	ts.TsSyncConsole(packet, client)
}

func (ts *Teamserver) TsAgentConsoleLocalCommand(agentId string, client string, cmdline string, message string, text string) {
	packet := CreateSpAgentLocalCommand(agentId, cmdline, message, text)
	ts.TsSyncConsole(packet, client)
}
