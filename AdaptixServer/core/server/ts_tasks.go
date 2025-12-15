package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"fmt"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsTaskRunningExists(agentId string, taskId string) bool {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		logs.Error("", "TsTaskUpdate: agent %v not found", agentId)
		return false
	}
	agent, _ := value.(*Agent)

	return agent.RunningTasks.Contains(taskId)
}

func (ts *Teamserver) TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		logs.Error("", "TsTaskCreate: agent %v not found", agentId)
		return
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return
	}

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agentId
	taskData.CommandLine = cmdline
	taskData.Client = client
	taskData.Computer = agent.Data.Computer
	taskData.StartDate = time.Now().Unix()
	if taskData.Completed {
		taskData.FinishDate = taskData.StartDate
	}

	taskData.User = agent.Data.Username
	if agent.Data.Impersonated != "" {
		taskData.User += fmt.Sprintf(" [%s]", agent.Data.Impersonated)
	}

	switch taskData.Type {

	case TYPE_TASK:
		if taskData.Sync {
			packet_task := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet_task)

			packet_console := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet_console)

			agent.OutConsole.Put(packet_console)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet_console)
		}
		agent.HostedTasks.Push(taskData)

	case TYPE_BROWSER:
		agent.HostedTasks.Push(taskData)

	case TYPE_JOB:
		if taskData.Sync {
			packet_task := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet_task)

			packet_console := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet_console)

			agent.OutConsole.Put(packet_console)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet_console)
		}
		agent.HostedTasks.Push(taskData)

	case TYPE_TUNNEL:
		if taskData.Sync {
			if taskData.Completed {
				agent.CompletedTasks.Put(taskData.TaskId, taskData)
			} else {
				agent.RunningTasks.Put(taskData.TaskId, taskData)
			}

			packet_task := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet_task)

			packet_console := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet_console)

			agent.OutConsole.Put(packet_console)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet_console)

			if taskData.Completed {
				_ = ts.DBMS.DbTaskInsert(taskData)
			}
		}

	case TYPE_PROXY_DATA:
		logs.Debug("", "----TYPE_PROXY_DATA----")

	default:
		break
	}
}

func (ts *Teamserver) TsTaskUpdate(agentId string, updateData adaptix.TaskData) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		logs.Error("", "TsTaskUpdate: agent %v not found", agentId)
		return
	}
	agent, _ := value.(*Agent)

	value, ok = agent.RunningTasks.Get(updateData.TaskId)
	if !ok {
		return
	}
	task, _ := value.(adaptix.TaskData)

	task.Data = []byte("")

	if task.Type == TYPE_JOB {
		updateData.AgentId = agentId

		if task.HookId != "" && task.Client != "" && ts.TsClientConnected(task.Client) {
			updateData.HookId = task.HookId

			hookJob := &HookJob{
				Job:       updateData,
				Processed: false,
				Sent:      false,
			}

			num := 0
			value2, ok := agent.RunningJobs.Get(task.TaskId)
			if ok {
				jobs := value2.(*safe.Slice)
				jobs.Put(hookJob)
				num = int(jobs.Len() - 1)
			} else {
				jobs := safe.NewSlice()
				jobs.Put(hookJob)
				agent.RunningJobs.Put(task.TaskId, jobs)
			}

			packet := CreateSpAgentTaskHook(updateData, num)
			ts.TsSyncClient(task.Client, packet)

		} else {
			if task.Sync {

				hookJob := &HookJob{
					Job:       updateData,
					Processed: true,
					Sent:      true,
				}

				value2, ok := agent.RunningJobs.Get(task.TaskId)

				if updateData.Completed {
					agent.RunningTasks.Delete(updateData.TaskId)

					if ok {
						jobs := value2.(*safe.Slice)
						jobs.Put(hookJob)
						jobs_array := jobs.CutArray()
						for _, job_value := range jobs_array {
							jobData := job_value.(*HookJob)
							if task.MessageType != CONSOLE_OUT_ERROR {
								task.MessageType = jobData.Job.MessageType
							}
							if task.Message == "" {
								task.Message = jobData.Job.Message
							}
							task.ClearText += jobData.Job.ClearText
						}

						agent.RunningJobs.Delete(task.TaskId)

					} else {
						task.MessageType = updateData.MessageType
						task.Message = updateData.Message
						task.ClearText = updateData.ClearText
					}

					task.FinishDate = updateData.FinishDate
					task.Completed = updateData.Completed

					agent.CompletedTasks.Put(task.TaskId, task)
					_ = ts.DBMS.DbTaskInsert(task)

				} else {
					if ok {
						jobs := value2.(*safe.Slice)
						jobs.Put(hookJob)
					} else {
						jobs := safe.NewSlice()
						jobs.Put(hookJob)
						agent.RunningJobs.Put(task.TaskId, jobs)
					}
				}

				packet_task_update := CreateSpAgentTaskUpdate(updateData)
				packet_console_update := CreateSpAgentConsoleTaskUpd(updateData)

				ts.TsSyncAllClients(packet_task_update)
				ts.TsSyncAllClients(packet_console_update)

				agent.OutConsole.Put(packet_console_update)
				_ = ts.DBMS.DbConsoleInsert(agentId, packet_console_update)
			}
		}

	} else if task.Type == TYPE_TUNNEL {
		agent.RunningTasks.Delete(updateData.TaskId)

		task.FinishDate = updateData.FinishDate
		task.Completed = updateData.Completed
		task.MessageType = updateData.MessageType

		var tmpTask = task
		tmpTask.Message = updateData.Message
		tmpTask.ClearText = updateData.ClearText

		if task.Message == "" {
			task.Message = updateData.Message
		}
		task.ClearText += updateData.ClearText

		if task.Sync {
			if task.Completed {
				agent.CompletedTasks.Put(task.TaskId, task)
				_ = ts.DBMS.DbTaskInsert(task)
			} else {
				agent.RunningTasks.Put(task.TaskId, task)
			}

			packet_task_update := CreateSpAgentTaskUpdate(tmpTask)
			packet_console_update := CreateSpAgentConsoleTaskUpd(tmpTask)

			ts.TsSyncAllClients(packet_task_update)
			ts.TsSyncAllClients(packet_console_update)

			agent.OutConsole.Put(packet_console_update)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet_console_update)
		}

	} else if task.Type == TYPE_TASK || task.Type == TYPE_BROWSER {
		agent.RunningTasks.Delete(updateData.TaskId)

		task.FinishDate = updateData.FinishDate
		task.Completed = updateData.Completed
		task.MessageType = updateData.MessageType
		task.Message = updateData.Message
		task.ClearText = updateData.ClearText

		if task.HookId != "" && task.Client != "" && ts.TsClientConnected(task.Client) {

			agent.RunningTasks.Put(task.TaskId, task)

			packet := CreateSpAgentTaskHook(task, 0)
			ts.TsSyncClient(task.Client, packet)

		} else {
			if task.Sync {
				if task.Completed {
					agent.CompletedTasks.Put(task.TaskId, task)
					_ = ts.DBMS.DbTaskInsert(task)
				} else {
					agent.RunningTasks.Put(task.TaskId, task)
				}

				packet := CreateSpAgentTaskUpdate(task)
				ts.TsSyncAllClients(packet)

				packet2 := CreateSpAgentConsoleTaskUpd(task)
				ts.TsSyncAllClients(packet2)

				agent.OutConsole.Put(packet2)
				_ = ts.DBMS.DbConsoleInsert(agentId, packet2)
			}
		}
	}
}

func (ts *Teamserver) TsTaskPostHook(hookData adaptix.TaskData, jobIndex int) error {
	value, ok := ts.agents.Get(hookData.AgentId)
	if !ok {
		return fmt.Errorf("agent %v not found", hookData.AgentId)
	}
	agent, _ := value.(*Agent)

	value, ok = agent.RunningTasks.Get(hookData.TaskId)
	if !ok {
		return fmt.Errorf("task %v not found", hookData.TaskId)
	}
	task, _ := value.(adaptix.TaskData)

	if task.HookId == "" || task.HookId != hookData.HookId || task.Client != hookData.Client || !ts.TsClientConnected(task.Client) {
		return fmt.Errorf("Operation not available")
	}

	if task.Type == TYPE_JOB {

		if task.Sync {

			value2, ok := agent.RunningJobs.Get(task.TaskId)
			if !ok {
				return fmt.Errorf("job %v not found", task.TaskId)
			}
			jobs := value2.(*safe.Slice)

			jobValue, ok := jobs.Get(uint(jobIndex))
			if !ok {
				return fmt.Errorf("job %v not found", task.TaskId)
			}
			jobData := jobValue.(*HookJob)

			jobData.Job.MessageType = hookData.MessageType
			jobData.Job.Message = hookData.Message
			jobData.Job.ClearText = hookData.ClearText
			jobData.Processed = true

			completed := false
			sent := true

			jobs.DirectLock()
			defer jobs.DirectUnlock()

			slice := jobs.DirectSlice()
			for i := 0; i < len(slice); i++ {

				hookJob := slice[i].(*HookJob)
				if hookJob.Job.Completed {
					completed = true
				}

				not_process_break := false

				hookJob.mu.Lock()
				if !hookJob.Sent {
					if hookJob.Processed {
						hookJob.Sent = true

						packet_task_update := CreateSpAgentTaskUpdate(hookJob.Job)
						packet_console_update := CreateSpAgentConsoleTaskUpd(hookJob.Job)

						ts.TsSyncAllClients(packet_task_update)
						ts.TsSyncAllClients(packet_console_update)

						agent.OutConsole.Put(packet_console_update)
						_ = ts.DBMS.DbConsoleInsert(task.AgentId, packet_console_update)
					} else {
						not_process_break = true
					}
				}
				if sent != false {
					sent = hookJob.Sent
				}

				hookJob.mu.Unlock()

				if not_process_break {
					break
				}
			}

			if completed && sent {
				agent.RunningTasks.Delete(task.TaskId)

				for i := 0; i < len(slice); i++ {
					hookJob := slice[i].(*HookJob)
					if task.MessageType != CONSOLE_OUT_ERROR {
						task.MessageType = hookJob.Job.MessageType
					}
					if task.Message == "" {
						task.Message = hookJob.Job.Message
					}
					task.ClearText += hookJob.Job.ClearText

					task.FinishDate = hookJob.Job.FinishDate
					task.Completed = hookJob.Job.Completed
				}

				agent.RunningJobs.Delete(task.TaskId)

				agent.CompletedTasks.Put(task.TaskId, task)
				_ = ts.DBMS.DbTaskInsert(task)
			}
		}

	} else if task.Type == TYPE_TUNNEL {

	} else if task.Type == TYPE_TASK || task.Type == TYPE_BROWSER {

		_, ok = agent.RunningTasks.GetDelete(hookData.TaskId)
		if !ok {
			return fmt.Errorf("task %v not found", hookData.TaskId)
		}

		task.MessageType = hookData.MessageType
		task.Message = hookData.Message
		task.ClearText = hookData.ClearText

		if task.Sync {
			if task.Completed {
				task.HookId = ""
				agent.CompletedTasks.Put(task.TaskId, task)
				_ = ts.DBMS.DbTaskInsert(task)
			} else {
				agent.RunningTasks.Put(task.TaskId, task)
			}

			packet := CreateSpAgentTaskUpdate(task)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpAgentConsoleTaskUpd(task)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(task.AgentId, packet2)
		}
	}
	return nil
}

func (ts *Teamserver) TsTaskCancel(agentId string, taskId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	found, retTask := agent.HostedTasks.RemoveIf(func(v interface{}) bool {
		task, ok := v.(adaptix.TaskData)
		return ok && task.TaskId == taskId
	})

	if found {
		task, ok := retTask.(adaptix.TaskData)
		if ok {
			packet := CreateSpAgentTaskRemove(task)
			ts.TsSyncAllClients(packet)
		}
	}
	return nil
}

func (ts *Teamserver) TsTaskDelete(agentId string, taskId string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	found, _ := agent.HostedTasks.FindIf(func(v interface{}) bool {
		task, ok := v.(*adaptix.TaskData)
		return ok && task.TaskId == taskId
	})
	if found {
		return fmt.Errorf("task %v in process", taskId)
	}

	value, ok = agent.RunningTasks.Get(taskId)
	if ok {
		return fmt.Errorf("task %v in process", taskId)
	}
	value, ok = agent.CompletedTasks.GetDelete(taskId)
	if !ok {
		return fmt.Errorf("task %v not found", taskId)
	}

	task := value.(adaptix.TaskData)
	_ = ts.DBMS.DbTaskDelete(task.TaskId, "")

	packet := CreateSpAgentTaskRemove(task)
	ts.TsSyncAllClients(packet)
	return nil
}

func (ts *Teamserver) TsTaskSave(taskData adaptix.TaskData) error {
	value, ok := ts.agents.Get(taskData.AgentId)
	if !ok {
		return fmt.Errorf("Agent %v not found", taskData.AgentId)
	}
	agent, _ := value.(*Agent)

	taskData.Type = TYPE_TASK
	taskData.TaskId, _ = krypt.GenerateUID(8)
	taskData.Computer = agent.Data.Computer
	taskData.User = agent.Data.Username
	taskData.StartDate = time.Now().Unix()
	taskData.FinishDate = taskData.StartDate
	taskData.Sync = true
	taskData.Completed = true

	agent.CompletedTasks.Put(taskData.TaskId, taskData)

	packet_task := CreateSpAgentTaskSync(taskData)
	ts.TsSyncAllClients(packet_task)

	_ = ts.DBMS.DbTaskInsert(taskData)

	return nil
}

type TaskListItem struct {
	TaskType   int    `json:"a_task_type"`
	TaskId     string `json:"a_task_id"`
	AgentId    string `json:"a_id"`
	Client     string `json:"a_client"`
	User       string `json:"a_user"`
	Computer   string `json:"a_computer"`
	CmdLine    string `json:"a_cmdline"`
	StartTime  int64  `json:"a_start_time"`
	FinishTime int64  `json:"a_finish_time"`
	MsgType    int    `json:"a_msg_type"`
	Message    string `json:"a_message"`
	Text       string `json:"a_text"`
	Completed  bool   `json:"a_completed"`
}

func (ts *Teamserver) TsTaskListCompleted(agentId string, limit int, offset int) (string, error) {
	if !ts.TsAgentIsExists(agentId) {
		return "", fmt.Errorf("agent %v not found", agentId)
	}

	tasks, err := ts.DBMS.DbTasksList(agentId, limit, offset)
	if err != nil {
		return "", err
	}

	items := make([]TaskListItem, 0, len(tasks))
	for _, task := range tasks {
		if !task.Completed {
			continue
		}
		items = append(items, TaskListItem{
			TaskType:   task.Type,
			TaskId:     task.TaskId,
			AgentId:    task.AgentId,
			Client:     task.Client,
			User:       task.User,
			Computer:   task.Computer,
			CmdLine:    task.CommandLine,
			StartTime:  task.StartDate,
			FinishTime: task.FinishDate,
			MsgType:    task.MessageType,
			Message:    task.Message,
			Text:       task.ClearText,
			Completed:  true,
		})
	}

	blob, err := json.Marshal(items)
	if err != nil {
		return "", err
	}
	return string(blob), nil
}

///// Get Tasks

func (ts *Teamserver) TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	var tasks []adaptix.TaskData
	tasksSize := 0

	/// TASKS QUEUE

	var sendTasks []string
	for {
		item, err := agent.HostedTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if tasksSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			if taskData.Sync || taskData.Type == TYPE_BROWSER {
				agent.RunningTasks.Put(taskData.TaskId, taskData)
			}
			sendTasks = append(sendTasks, taskData.TaskId)
			tasksSize += len(taskData.Data)
		} else {
			agent.HostedTasks.PushFront(taskData)
			break
		}
	}
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	for {
		item, err := agent.HostedTunnelTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if tasksSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			tasksSize += len(taskData.Data)
		} else {
			agent.HostedTunnelTasks.PushFront(taskData)
			break
		}
	}

	/// TUNNELS QUEUE

	for {
		item, err := agent.HostedTunnelData.Pop()
		if err != nil {
			break
		}
		taskDataTunnel := item.(adaptix.TaskDataTunnel)

		if tasksSize+len(taskDataTunnel.Data.Data) < availableSize {
			tasks = append(tasks, taskDataTunnel.Data)
			tasksSize += len(taskDataTunnel.Data.Data)
		} else {
			agent.HostedTunnelData.PushFront(taskDataTunnel)
			break
		}
	}

	/// PIVOTS QUEUE

	for i := uint(0); i < agent.PivotChilds.Len(); i++ {
		value, ok = agent.PivotChilds.Get(i)
		if ok {
			pivotData := value.(*adaptix.PivotData)
			lostSize := availableSize - tasksSize
			if availableSize > 0 {
				data, err := ts.TsAgentGetHostedAll(pivotData.ChildAgentId, lostSize)
				if err != nil {
					continue
				}
				pivotTaskData, err := ts.Extender.ExAgentPivotPackData(agent.Data.Name, pivotData.PivotId, data)
				if err != nil {
					continue
				}
				tasks = append(tasks, pivotTaskData)
				tasksSize += len(pivotTaskData.Data)
			}
		} else {
			break
		}
	}

	return tasks, nil
}

func (ts *Teamserver) TsTaskGetAvailableTasks(agentId string, availableSize int) ([]adaptix.TaskData, int, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, 0, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	var tasks []adaptix.TaskData
	tasksSize := 0

	/// TASKS QUEUE

	var sendTasks []string
	for {
		item, err := agent.HostedTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if tasksSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			if taskData.Sync || taskData.Type == TYPE_BROWSER {
				agent.RunningTasks.Put(taskData.TaskId, taskData)
			}
			sendTasks = append(sendTasks, taskData.TaskId)
			tasksSize += len(taskData.Data)
		} else {
			agent.HostedTasks.PushFront(taskData)
			break
		}
	}
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	for {
		item, err := agent.HostedTunnelTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if tasksSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			tasksSize += len(taskData.Data)
		} else {
			agent.HostedTunnelTasks.PushFront(taskData)
			break
		}
	}

	return tasks, tasksSize, nil
}

func (ts *Teamserver) TsTaskGetAvailableTasksCount(agentId string, maxCount int, availableSize int) ([]adaptix.TaskData, int, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, 0, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	var tasks []adaptix.TaskData
	tasksSize := 0

	/// TASKS QUEUE

	var sendTasks []string
	for i := 0; i < maxCount; i++ {
		item, err := agent.HostedTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if tasksSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			if taskData.Sync || taskData.Type == TYPE_BROWSER {
				agent.RunningTasks.Put(taskData.TaskId, taskData)
			}
			sendTasks = append(sendTasks, taskData.TaskId)
			tasksSize += len(taskData.Data)
		} else {
			agent.HostedTasks.PushFront(taskData)
			break
		}
	}
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	return tasks, tasksSize, nil
}

/// Get Pivot Tasks

func (ts *Teamserver) TsTasksPivotExists(agentId string, first bool) bool {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return false
	}
	agent := value.(*Agent)

	if !first {
		if agent.HostedTasks.Len() > 0 || agent.HostedTunnelTasks.Len() > 0 || agent.HostedTunnelData.Len() > 0 {
			return true
		}
	}

	for i := uint(0); i < agent.PivotChilds.Len(); i++ {
		value, ok = agent.PivotChilds.Get(i)
		if ok {
			pivotData := value.(*adaptix.PivotData)
			if ts.TsTasksPivotExists(pivotData.ChildAgentId, false) {
				return true
			}
		}
	}
	return false
}

func (ts *Teamserver) TsTaskGetAvailablePivotAll(agentId string, availableSize int) ([]adaptix.TaskData, error) {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}
	agent, _ := value.(*Agent)

	var tasks []adaptix.TaskData
	tasksSize := 0

	/// PIVOTS QUEUE

	for i := uint(0); i < agent.PivotChilds.Len(); i++ {
		value, ok = agent.PivotChilds.Get(i)
		if ok {
			pivotData := value.(*adaptix.PivotData)
			lostSize := availableSize - tasksSize
			if availableSize > 0 {
				data, err := ts.TsAgentGetHostedAll(pivotData.ChildAgentId, lostSize)
				if err != nil {
					continue
				}
				pivotTaskData, err := ts.Extender.ExAgentPivotPackData(agent.Data.Name, pivotData.PivotId, data)
				if err != nil {
					continue
				}
				tasks = append(tasks, pivotTaskData)
				tasksSize += len(pivotTaskData.Data)
			}
		} else {
			break
		}
	}

	return tasks, nil
}
