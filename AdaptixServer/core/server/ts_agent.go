package server

import (
	"AdaptixServer/core/eventing"
	isvalid "AdaptixServer/core/utils/valid"
	"context"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsAgentGenID() int64 {
	return ts.IdGen.Next("agent")
}

func (ts *Teamserver) TsAgentList() (string, error) {
	var agents []adaptix.AgentData
	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		agents = append(agents, agent.GetData())
		return true
	})

	jsonAgents, err := json.Marshal(agents)
	if err != nil {
		return "", err
	}
	return string(jsonAgents), nil
}

func (ts *Teamserver) TsAgentIsExists(agentId int64) bool {
	return ts.Agents.Contains(agentId)
}

func (ts *Teamserver) TsAgentIdByUID(uid []byte) (int64, bool) {
	if len(uid) == 0 {
		return 0, false
	}
	id, ok := ts.agentsUid.Get(hex.EncodeToString(uid))
	if ok {
		return id, true
	}
	return 0, false
}

func (ts *Teamserver) TsAgentGetById(agentId int64) (adaptix.AgentData, bool) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return adaptix.AgentData{}, false
	}
	return agent.GetData(), true
}

func (ts *Teamserver) TsAgentCreate(agentCrc string, agentUid []byte, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error) {
	if beat == nil {
		return adaptix.AgentData{}, fmt.Errorf("agent %x does not register", agentUid)
	}

	agentName, ok := ts.wm_agent_types.Get(agentCrc)
	if !ok {
		return adaptix.AgentData{}, fmt.Errorf("agent type %v does not exists", agentCrc)
	}

	agentData, agentFuncs, err := ts.Extender.ExAgentCreate(agentName, beat)
	if err != nil {
		return adaptix.AgentData{}, err
	}

	agentData.Id = ts.IdGen.Next("agent")

	if len(agentData.UID) == 0 && len(agentUid) > 0 {
		agentData.UID = agentUid
	}
	if len(agentData.UID) > 0 {
		_, exists := ts.agentsUid.Get(hex.EncodeToString(agentData.UID))
		if exists {
			return adaptix.AgentData{}, fmt.Errorf("agent with uid %x already exists", agentData.UID)
		}
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

	listenerData, ok := ts.listeners.Get(listenerName)
	if !ok {
		return agentData, fmt.Errorf("listener %v does not exists", listenerName)
	}

	regName := listenerData.RegName
	_, ok = ts.listener_configs.Get(regName)
	if !ok {
		return agentData, fmt.Errorf("listener %v does not register", regName)
	}

	agent := adaptix.NewAgent(agentData, agentFuncs)

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
	if len(agentData.UID) > 0 {
		ts.agentsUid.Put(hex.EncodeToString(agentData.UID), agentData.Id)
	}

	packetNew := CreateSpAgentNew(agentData)
	ts.TsSyncAllClientsWithCategory(packetNew, SyncCategoryAgents)

	tid, err := ts.TsTargetsCreateAlive(agentData)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "failed to create target for agent %d: %v", agentData.Id, err)
	}
	agent.UpdateData(func(d *adaptix.AgentData) {
		d.TargetId = tid
	})

	err = ts.DBMS.DbAgentInsert(agent.GetData())
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
	}

	ts.TsNotifyAgent(false, agent.GetData())

	// --- POST HOOK ---
	postEvent := &eventing.EventDataAgentNew{Agent: agent.GetData(), Restore: false}
	ts.EventManager.EmitAsync(eventing.EventAgentNew, postEvent)
	// -----------------

	return agent.GetData(), nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId int64, clientName string, hookId string, handlerId string, cmdline string, ui bool, args map[string]any) error {
	if !ts.agent_configs.Contains(agentName) {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}
	if !agent.IsActive() {
		return fmt.Errorf("agent '%v' not active", agentId)
	}
	if agent.IsRemoved() {
		return adaptix.ErrAgentRemoved
	}

	taskData, messageData, err := agent.Fn.CreateCommand(agent.GetData(), args)
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
			ts.TsAgentConsoleOutput(agentId, clientName, messageData.Status, messageData.Message, messageData.Text, false)
		}
	}

	return nil
}

func (ts *Teamserver) TsAgentProcessData(agentId int64, bodyData []byte) error {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}

	var markChanged bool
	agent.UpdateData(func(d *adaptix.AgentData) {
		if d.Mark == "Inactive" {
			d.Mark = ""
			markChanged = true
		}
	})

	if markChanged {
		err := ts.DBMS.DbAgentUpdate(agent.GetData())
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
		}

		// --- POST HOOK ---
		postEvent := &eventing.EventDataAgentActivate{Agent: agent.GetData()}
		ts.EventManager.EmitAsync(eventing.EventAgentActivate, postEvent)
		// -----------------
	}

	if !agent.GetData().Async {
		agent.UpdateData(func(d *adaptix.AgentData) {
			d.LastTick = int(time.Now().Unix())
		})
	}

	if len(bodyData) != 0 {
		return agent.ProcessData(bodyData)
	}

	return nil
}

/// Get Tasks

func (ts *Teamserver) TsAgentBuildEmptyTasks(agentId int64) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	return agent.PackTasks(nil)
}

func (ts *Teamserver) packAgentTasks(agent *adaptix.Agent, tasks []adaptix.TaskData) ([]byte, error) {
	ts.TaskManager.DispatchPreEncode(agent, tasks)

	respData, err := agent.PackTasks(tasks)
	if err != nil {
		return nil, err
	}

	ts.TaskManager.DispatchPostEncode(agent, tasks)
	return respData, nil
}

func (ts *Teamserver) TsAgentGetHostedAll(agentId int64, maxDataSize int) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	queueLen := agent.HostedQueue.Len()
	pivotTasksExists := false
	if agent.PivotChilds.Len() > 0 {
		pivotTasksExists = ts.TsTasksPivotExists(agentId, true)
	}

	if queueLen > 0 || pivotTasksExists {

		tasks, err := ts.TsTaskGetAvailableAll(agentId, maxDataSize)
		if err != nil {
			return nil, err
		}

		visibleCount := countOperatorVisibleTasks(tasks)
		if visibleCount > 0 {
			ts.TsAgentConsoleOutput(agentId, "", CONSOLE_OUT_INFO, fmt.Sprintf("Agent polled — %d task(s) to send", visibleCount), "", false)
		}

		respData, err := ts.packAgentTasks(agent, tasks)
		if err != nil {
			return nil, err
		}

		return respData, nil
	}

	return []byte(""), nil
}

func countOperatorVisibleTasks(tasks []adaptix.TaskData) int {
	n := 0
	for i := range tasks {
		switch tasks[i].Type {
		case adaptix.TASK_TYPE_TASK, adaptix.TASK_TYPE_JOB:
			n++
		}
	}
	return n
}

func (ts *Teamserver) TsAgentGetHostedTasks(agentId int64, maxDataSize int) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	if agent.HostedQueue.Len() == 0 {
		return []byte(""), nil
	}

	tasks, _, err := ts.TsTaskGetAvailableTasks(agentId, 0, maxDataSize)
	if err != nil {
		return nil, err
	}

	visibleCount := countOperatorVisibleTasks(tasks)
	if visibleCount > 0 {
		ts.TsAgentConsoleOutput(agentId, "", CONSOLE_OUT_INFO, fmt.Sprintf("Agent polled — %d task(s) to send", visibleCount), "", false)
	}

	respData, err := ts.packAgentTasks(agent, tasks)
	if err != nil {
		return nil, err
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentGetHostedTasksCount(agentId int64, count int, maxDataSize int) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}

	if agent.HostedQueue.Len() == 0 {
		return []byte(""), nil
	}

	tasks, _, err := ts.TsTaskGetAvailableTasks(agentId, count, maxDataSize)
	if err != nil {
		return nil, err
	}

	visibleCount := countOperatorVisibleTasks(tasks)
	if visibleCount > 0 {
		ts.TsAgentConsoleOutput(agentId, "", CONSOLE_OUT_INFO, fmt.Sprintf("Agent polled — %d task(s) to send", visibleCount), "", false)
	}

	respData, err := ts.packAgentTasks(agent, tasks)
	if err != nil {
		return nil, err
	}

	return respData, nil
}

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
	agent, ok := ts.Agents.Get(newAgentData.Id)
	if !ok {
		return errors.New("agent does not exist")
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
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncStateWithCategory(packetNew, fmt.Sprintf("agent:%d", agentData.Id), SyncCategoryAgents)

	return nil
}

func (ts *Teamserver) TsAgentUpdateDataPartial(agentId int64, updateData interface{}) error {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}

	syncPacket := SyncPackerAgentUpdate{
		SpType: TYPE_AGENT_UPDATE,
		Id:     agentId,
	}

	updated := ts.applyAgentUpdate(agent, updateData, &syncPacket)
	if !updated {
		return nil
	}

	err := ts.DBMS.DbAgentUpdate(agent.GetData())
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
		return nil
	}

	ts.TsSyncAllClients(syncPacket)

	return nil
}

func (ts *Teamserver) applyAgentUpdate(agent *adaptix.Agent, updateData interface{}, syncPacket *SyncPackerAgentUpdate) bool {
	updated := false

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
		CustomData   *string `json:"custom_data,omitempty"`
		SessionKey   *[]byte `json:"a_session_key,omitempty"`
		Sleep        *uint   `json:"sleep,omitempty"`
		Jitter       *uint   `json:"jitter,omitempty"`
		WorkingTime  *int    `json:"working_time,omitempty"`
		KillDate     *int    `json:"kill_date,omitempty"`
	}

	jsonBytes, err := json.Marshal(updateData)
	if err != nil {
		return false
	}

	var fields fieldAccessor
	if err := json.Unmarshal(jsonBytes, &fields); err != nil {
		return false
	}

	agent.UpdateData(func(d *adaptix.AgentData) {
		if fields.InternalIP != nil {
			d.InternalIP = *fields.InternalIP
			syncPacket.InternalIP = fields.InternalIP
			updated = true
		}
		if fields.ExternalIP != nil {
			d.ExternalIP = *fields.ExternalIP
			syncPacket.ExternalIP = fields.ExternalIP
			updated = true
		}
		if fields.GmtOffset != nil {
			d.GmtOffset = *fields.GmtOffset
			syncPacket.GmtOffset = fields.GmtOffset
			updated = true
		}
		if fields.ACP != nil {
			d.ACP = *fields.ACP
			syncPacket.ACP = fields.ACP
			updated = true
		}
		if fields.OemCP != nil {
			d.OemCP = *fields.OemCP
			syncPacket.OemCP = fields.OemCP
			updated = true
		}
		if fields.Pid != nil {
			d.Pid = *fields.Pid
			syncPacket.Pid = fields.Pid
			updated = true
		}
		if fields.Tid != nil {
			d.Tid = *fields.Tid
			syncPacket.Tid = fields.Tid
			updated = true
		}
		if fields.Arch != nil {
			d.Arch = *fields.Arch
			syncPacket.Arch = fields.Arch
			updated = true
		}
		if fields.Elevated != nil {
			d.Elevated = *fields.Elevated
			syncPacket.Elevated = fields.Elevated
			updated = true
		}
		if fields.Process != nil {
			d.Process = *fields.Process
			syncPacket.Process = fields.Process
			updated = true
		}
		if fields.Os != nil {
			d.Os = *fields.Os
			syncPacket.Os = fields.Os
			updated = true
		}
		if fields.OsDesc != nil {
			d.OsDesc = *fields.OsDesc
			syncPacket.OsDesc = fields.OsDesc
			updated = true
		}
		if fields.Domain != nil {
			d.Domain = *fields.Domain
			syncPacket.Domain = fields.Domain
			updated = true
		}
		if fields.Computer != nil {
			d.Computer = *fields.Computer
			syncPacket.Computer = fields.Computer
			updated = true
		}
		if fields.Username != nil {
			d.Username = *fields.Username
			syncPacket.Username = fields.Username
			updated = true
		}
		if fields.Impersonated != nil {
			d.Impersonated = *fields.Impersonated
			syncPacket.Impersonated = fields.Impersonated
			updated = true
		}
		if fields.Tags != nil {
			d.Tags = *fields.Tags
			syncPacket.Tags = fields.Tags
			updated = true
		}
		if fields.Listener != nil {
			d.Listener = *fields.Listener
			syncPacket.Listener = fields.Listener
			updated = true
		}
		if fields.Mark != nil {
			if d.Mark != "Terminated" && d.Mark != *fields.Mark {
				d.Mark = *fields.Mark
				syncPacket.Mark = fields.Mark
				if *fields.Mark == "Disconnect" {
					d.LastTick = int(time.Now().Unix())
				}
				updated = true
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
			updated = true
		}
		if fields.CustomData != nil {
			d.CustomData = []byte(*fields.CustomData)
			updated = true
		}
		if fields.SessionKey != nil {
			d.SessionKey = *fields.SessionKey
			updated = true
		}
		if fields.Sleep != nil {
			d.Sleep = *fields.Sleep
			syncPacket.Sleep = fields.Sleep
			updated = true
		}
		if fields.Jitter != nil {
			d.Jitter = *fields.Jitter
			syncPacket.Jitter = fields.Jitter
			updated = true
		}
		if fields.WorkingTime != nil {
			d.WorkingTime = *fields.WorkingTime
			syncPacket.WorkingTime = fields.WorkingTime
			updated = true
		}
		if fields.KillDate != nil {
			d.KillDate = *fields.KillDate
			syncPacket.KillDate = fields.KillDate
			updated = true
		}
	})

	return updated
}

func (ts *Teamserver) cleanupAgentResources(agentId int64) {

	/// Clear Downloads

	var downloads []int64
	ts.downloads.ForEachFast(func(key int64, downloadData adaptix.TransferData) bool {
		if downloadData.AgentId == agentId && downloadData.State != adaptix.TRANSFER_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, adaptix.TRANSFER_STATE_CANCELED)
	}

	/// Clear Uploads

	var uploads []int64
	ts.uploads.ForEachFast(func(key int64, uploadData adaptix.TransferData) bool {
		if uploadData.AgentId == agentId {
			uploads = append(uploads, uploadData.FileId)
		}
		return true
	})
	for _, id := range uploads {
		_ = ts.closeUploadForce(id, adaptix.TRANSFER_STATE_CANCELED)
	}

	/// Clear Tunnels

	var tunnelIds []int64
	ts.TunnelManager.ForEachTunnel(func(key int64, tunnel *Tunnel) bool {
		if tunnel.Data.AgentId == agentId {
			tunnelIds = append(tunnelIds, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnelIds {
		_ = ts.TsTunnelStop(id)
	}

	/// Clear Terminals

	var terminalIds []int64
	ts.terminals.ForEachFast(func(key int64, term *Terminal) bool {
		if term.agentId == agentId {
			terminalIds = append(terminalIds, term.TerminalId)
		}
		return true
	})
	for _, id := range terminalIds {
		_ = ts.TsTerminalConnClose(id, "agent cleanup")
	}

	/// Clear Pivots

	agent, ok := ts.Agents.Get(agentId)
	if ok {
		if agent.GetPivotParent() != nil {
			_ = ts.TsPivotDelete(agent.GetPivotParent().PivotId)
		}

		var pivots []string
		for value := range agent.PivotChilds.Iterator() {
			pivotId := value.Item.(*adaptix.PivotData).PivotId
			pivots = append(pivots, pivotId)
		}
		for _, pivotId := range pivots {
			_ = ts.TsPivotDelete(pivotId)
		}
	}
}

func (ts *Teamserver) TsAgentTerminate(agentId int64, terminateTaskId int64) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentTerminate{AgentId: agentId, TaskId: terminateTaskId}
	if !ts.EventManager.Emit(eventing.EventAgentTerminate, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("agent does not exist")
	}
	agent.SetActive(false)
	agent.UpdateData(func(d *adaptix.AgentData) {
		d.Mark = "Terminated"
	})

	ts.cleanupAgentResources(agentId)

	/// Clear HostedQueue
	for {
		item, err := agent.HostedQueue.Pop()
		if err != nil {
			break
		}
		task := item.(adaptix.TaskData)
		packet := CreateSpAgentTaskRemove(task)
		ts.TsSyncAllClients(packet)
	}

	/// Clear TasksRunning

	tasksRunning := agent.RunningTasks.CutMap()
	for _, task := range tasksRunning {
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

	/// Update

	agentData := agent.GetData()
	err := ts.DBMS.DbAgentUpdate(agentData)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agentData)
	ts.TsSyncStateWithCategory(packetNew, fmt.Sprintf("agent:%d", agentId), SyncCategoryAgents)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataAgentTerminate{AgentId: agentId, TaskId: terminateTaskId}
	ts.EventManager.EmitAsync(eventing.EventAgentTerminate, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsAgentConsoleRemove(agentId int64) error {
	_, ok := ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}
	_ = ts.DBMS.DbConsoleDelete(agentId)

	return nil
}

func (ts *Teamserver) TsSetAgentDeliveryFunc(agentId int64, fn adaptix.DeliveryFunc) {
	agent, ok := ts.Agents.Get(agentId)
	if ok {
		agent.Fn.Delivery = fn
	}
}

func (ts *Teamserver) TsRemoveAgentDeliveryFunc(agentId int64) {
	agent, ok := ts.Agents.Get(agentId)
	if ok {
		agent.Fn.Delivery = nil
	}
}

func (ts *Teamserver) TsGetAgentDeliveryFunc(AgentId int64) adaptix.DeliveryFunc {
	agent, ok := ts.Agents.Get(AgentId)
	if !ok {
		return nil
	}
	return agent.Fn.Delivery
}

func (ts *Teamserver) TsAgentRemove(agentId int64) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentRemove{}
	agent, ok := ts.Agents.Get(agentId)
	if ok {
		preEvent.Agent = agent.GetData()
	}
	if !ts.EventManager.Emit(eventing.EventAgentRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent.MarkRemoved()
	ts.cleanupAgentResources(agentId)
	uid := agent.GetData().UID
	if len(uid) > 0 {
		ts.agentsUid.Delete(hex.EncodeToString(uid))
	}
	ts.Agents.Delete(agentId)

	err := ts.DBMS.DbAgentDelete(agentId)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "agent", "%s", err.Error())
	} else {
		_ = ts.DBMS.DbTaskDelete(0, agentId)
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

func (ts *Teamserver) TsAgentSetTick(agentId int64, listenerName string) error {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentId)
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
			ts.TsSyncStateWithCategory(packet, fmt.Sprintf("agent:%d", agentId), SyncCategoryAgents)
			_ = ts.DBMS.DbAgentUpdate(updatedAgentData)
		} else {
			agent.UpdateData(func(d *adaptix.AgentData) {
				d.LastTick = int(time.Now().Unix())
			})
			_ = ts.DBMS.DbAgentTick(agent.GetData())
		}
		ts.tickedAgents.Add(agentId)
		select {
		case ts.tickNotify <- struct{}{}:
		default:
		}
	} else if listenerChanged {
		agent.UpdateData(func(d *adaptix.AgentData) {
			d.Listener = listenerName
		})
		updatedAgentData := agent.GetData()
		packet := CreateSpAgentUpdate(updatedAgentData)
		ts.TsSyncStateWithCategory(packet, fmt.Sprintf("agent:%d", agentId), SyncCategoryAgents)
		_ = ts.DBMS.DbAgentUpdate(updatedAgentData)
	}
	return nil
}

/// Sync

func (ts *Teamserver) TsAgentTickUpdate(ctx context.Context) {
	timer := time.NewTicker(800 * time.Millisecond)
	defer timer.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ts.tickNotify:
		case <-timer.C:
		}

		agentSlice := ts.tickedAgents.Drain()

		if len(agentSlice) > 0 {
			packetTick := CreateSpAgentTick(agentSlice)
			ts.TsSyncStateWithCategory(packetTick, "tick", SyncCategoryAgents)
		}
	}
}

/// Crypt

func (ts *Teamserver) TsAgentEncryptData(agentId int64, data []byte) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	return agent.EncryptData(data)
}

func (ts *Teamserver) TsAgentDecryptData(agentId int64, data []byte) ([]byte, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	return agent.DecryptData(data)
}

/// Console

func (ts *Teamserver) TsAgentConsoleOutput(agentId int64, client string, messageType int, message string, clearText string, store bool) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncConsole(packet, "", client)

	if store {
		_ = ts.DBMS.DbConsoleInsert(agentId, client, packet)
	}
}

func (ts *Teamserver) TsAgentConsoleOutputClient(agentId int64, client string, messageType int, message string, clearText string) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncConsole(packet, client, client)
}

func (ts *Teamserver) TsAgentConsoleErrorCommand(agentId int64, client string, cmdline string, message string, HookId string, HandlerId string) {
	packet := CreateSpAgentErrorCommand(agentId, cmdline, message, HookId, HandlerId)
	ts.TsSyncConsole(packet, client, client)
}

func (ts *Teamserver) TsAgentConsoleLocalCommand(agentId int64, client string, cmdline string, message string, text string) {
	packet := CreateSpAgentLocalCommand(agentId, cmdline, message, text)
	ts.TsSyncConsole(packet, client, client)
}
