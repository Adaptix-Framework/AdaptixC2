package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"AdaptixServer/core/utils/tformat"
	isvalid "AdaptixServer/core/utils/valid"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
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
		OutConsole:         safe.NewSlice(),
		HostedTasks:        safe.NewSafeQueue(0x100),
		HostedTunnelTasks:  safe.NewSafeQueue(0x100),
		HostedTunnelData:   safe.NewSafeQueue(0x1000),
		InflightDeliveries: safe.NewMap(),
		RunningTasks:       safe.NewMap(),
		RunningJobs:        safe.NewMap(),
		CompletedTasks:     safe.NewMap(),
		PivotParent:        nil,
		PivotChilds:        safe.NewSlice(),
		Tick:               false,
		Active:             true,
	}
	agent.SetData(agentData)

	ts.Agents.Put(agentData.Id, agent)

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

	agent, err := ts.getAgent(agentId)
	if err != nil {
		return err
	}
	if !agent.Active {
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
	}

	if len(bodyData) > 4 {
		_, err := ts.Extender.ExAgentProcessData(agent.GetData(), bodyData)
		return err
	}

	return nil
}

/// Get Tasks

type inflightDelivery struct {
	nonce     uint32
	createdAt time.Time
	attempts  int
	tasks     []adaptix.TaskData
}

func (ts *Teamserver) tsGetOrCreateInflight(agent *Agent, maxDataSize int) ([]byte, uint32, error) {
	// Prefer resending the oldest inflight delivery (for listener restart / downFrags loss).
	var (
		oldestKey string
		oldest    *inflightDelivery
	)
	agent.InflightDeliveries.ForEach(func(key string, value interface{}) bool {
		d, ok := value.(*inflightDelivery)
		if !ok || d == nil {
			return true
		}
		if oldest == nil || d.createdAt.Before(oldest.createdAt) {
			oldest = d
			oldestKey = key
		}
		return true
	})
	if oldest != nil {
		oldest.attempts++
		agent.InflightDeliveries.Put(oldestKey, oldest)
		respData, err := ts.Extender.ExAgentPackData(agent.GetData(), oldest.tasks)
		return respData, oldest.nonce, err
	}

	// No inflight; create a new delivery if there is something to send.
	tasksCount := agent.HostedTasks.Len()
	tunnelConnectCount := agent.HostedTunnelTasks.Len()
	tunnelTasksCount := agent.HostedTunnelData.Len()
	pivotTasksExists := false
	if agent.PivotChilds.Len() > 0 {
		pivotTasksExists = ts.TsTasksPivotExists(agent.GetData().Id, true)
	}
	if !(tasksCount > 0 || tunnelConnectCount > 0 || tunnelTasksCount > 0 || pivotTasksExists) {
		return []byte(""), 0, nil
	}

	tasks, err := ts.TsTaskGetAvailableAll(agent.GetData().Id, maxDataSize)
	if err != nil {
		return nil, 0, err
	}
	if len(tasks) == 0 {
		return []byte(""), 0, nil
	}

	// Create delivery nonce (8 hex chars -> uint32).
	uid, err := krypt.GenerateUID(8)
	if err != nil {
		return nil, 0, err
	}
	n64, err := strconv.ParseUint(uid, 16, 32)
	if err != nil {
		return nil, 0, err
	}
	nonce := uint32(n64)
	key := fmt.Sprintf("%08x", nonce)

	d := &inflightDelivery{nonce: nonce, createdAt: time.Now(), attempts: 1, tasks: tasks}
	agent.InflightDeliveries.Put(key, d)

	respData, err := ts.Extender.ExAgentPackData(agent.GetData(), tasks)
	if err != nil {
		// If pack fails, roll tasks back to HostedTasks to avoid loss.
		for i := len(tasks) - 1; i >= 0; i-- {
			agent.HostedTasks.PushFront(tasks[i])
		}
		agent.InflightDeliveries.Delete(key)
		return nil, 0, err
	}

	if tasksCount > 0 {
		message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
		ts.TsAgentConsoleOutput(agent.GetData().Id, CONSOLE_OUT_INFO, message, "", false)
	}
	return respData, nonce, nil
}

// TsAgentGetHostedAllDelivery returns packed tasks plus a delivery nonce which must be ACKed
// by the listener once the agent confirms full receipt (e.g., via DNS/DoH HB ackTaskNonce).
func (ts *Teamserver) TsAgentGetHostedAllDelivery(agentId string, maxDataSize int) ([]byte, uint32, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, 0, err
	}
	return ts.tsGetOrCreateInflight(agent, maxDataSize)
}

// TsAgentAckDelivery is called by a listener when the agent has ACKed a full delivery.
func (ts *Teamserver) TsAgentAckDelivery(agentId string, deliveryNonce uint32) error {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return err
	}

	key := fmt.Sprintf("%08x", deliveryNonce)
	v, found := agent.InflightDeliveries.GetDelete(key)
	if !found {
		return nil
	}
	d, ok := v.(*inflightDelivery)
	if !ok || d == nil {
		return nil
	}

	// Only now promote sync/browser tasks to RunningTasks.
	for _, taskData := range d.tasks {
		if taskData.Sync || taskData.Type == TYPE_BROWSER {
			agent.RunningTasks.Put(taskData.TaskId, taskData)
		}
	}
	return nil
}

// TsInflightRequeueLoop requeues timed-out deliveries back into HostedTasks.
func (ts *Teamserver) TsInflightRequeueLoop() {
	const (
		tickSeconds      = 10
		deliveryTimeout  = 90 * time.Second
		maxAttempt       = 10
		maxRequeuePerRun = 64
	)

	ticker := time.NewTicker(time.Duration(tickSeconds) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		requeued := 0
		now := time.Now()

		ts.Agents.ForEach(func(_ string, value interface{}) bool {
			if requeued >= maxRequeuePerRun {
				return false
			}
			agent, ok := value.(*Agent)
			if !ok || agent == nil {
				return true
			}
			// collect keys first to avoid holding safe.Map locks while pushing tasks
			var toRequeue []string
			agent.InflightDeliveries.ForEach(func(k string, v interface{}) bool {
				d, ok := v.(*inflightDelivery)
				if !ok || d == nil {
					toRequeue = append(toRequeue, k)
					return true
				}
				age := now.Sub(d.createdAt)
				if age >= deliveryTimeout || d.attempts >= maxAttempt {
					toRequeue = append(toRequeue, k)
				}
				return true
			})

			for _, k := range toRequeue {
				if requeued >= maxRequeuePerRun {
					break
				}
				v, found := agent.InflightDeliveries.GetDelete(k)
				if !found {
					continue
				}
				d, ok := v.(*inflightDelivery)
				if !ok || d == nil {
					continue
				}
				// requeue tasks to the front (reverse order preserves original order)
				for i := len(d.tasks) - 1; i >= 0; i-- {
					agent.HostedTasks.PushFront(d.tasks[i])
				}
				requeued++
			}

			return true
		})

		_ = requeued
	}
}

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
		if downloadData.AgentId == agentId && downloadData.State != DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, DOWNLOAD_STATE_CANCELED)
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
		if downloadData.AgentId == agentId && downloadData.State != DOWNLOAD_STATE_FINISHED {
			downloads = append(downloads, downloadData.FileId)
		}
		return true
	})
	for _, id := range downloads {
		_ = ts.TsDownloadClose(id, DOWNLOAD_STATE_CANCELED)
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
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsAgentSetTick(agentId string) error {
	value, ok := ts.Agents.Get(agentId)
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
