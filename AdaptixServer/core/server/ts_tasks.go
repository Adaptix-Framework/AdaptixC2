package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"bytes"
	"encoding/json"
	"fmt"
	"time"
)

func (ts *Teamserver) TsTaskCreate(agentId string, cmdline string, client string, taskObject []byte) {
	var (
		agent    *Agent
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)
	err = json.Unmarshal(taskObject, &taskData)
	if err != nil {
		logs.Error("", "TsTaskCreate: %v", err.Error())
		return
	}

	value, ok = ts.agents.Get(agentId)
	if !ok {
		logs.Error("", "TsTaskCreate: agent %v not found", agentId)
		return
	}
	agent = value.(*Agent)
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
			packet := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet2)
		}
		agent.TasksQueue.Put(taskData)

	case TYPE_BROWSER:
		agent.TasksQueue.Put(taskData)

	case TYPE_JOB:
		if taskData.Sync {
			packet := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet2)
		}
		agent.TasksQueue.Put(taskData)

	case TYPE_TUNNEL:
		if taskData.Completed {
			agent.CompletedTasks.Put(taskData.TaskId, taskData)
		} else {
			agent.RunningTasks.Put(taskData.TaskId, taskData)
		}

		if taskData.Sync {
			packet := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpAgentConsoleTaskSync(taskData)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet2)

			if taskData.Completed {
				_ = ts.DBMS.DbTaskInsert(taskData)
			}
		}

	case TYPE_PROXY_DATA:
		agent.TunnelQueue.Put(taskData)

	default:
		break
	}
}

func (ts *Teamserver) TsTaskUpdate(agentId string, taskObject []byte) {
	var (
		agent    *Agent
		task     adaptix.TaskData
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)
	err = json.Unmarshal(taskObject, &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(agentId)
	if !ok {
		logs.Error("", "TsTaskUpdate: agent %v not found", agentId)
		return
	}
	agent = value.(*Agent)

	value, ok = agent.RunningTasks.GetDelete(taskData.TaskId)
	if !ok {
		return
	}
	task = value.(adaptix.TaskData)

	task.Data = []byte("")
	task.FinishDate = taskData.FinishDate
	task.Completed = taskData.Completed

	if task.Type == TYPE_JOB {
		if task.MessageType != CONSOLE_OUT_ERROR {
			task.MessageType = taskData.MessageType
		}

		var oldMessage string
		if task.Message == "" {
			oldMessage = taskData.Message
		} else {
			oldMessage = task.Message
		}

		oldText := task.ClearText

		task.Message = taskData.Message
		task.ClearText = taskData.ClearText

		packet := CreateSpAgentTaskUpdate(task)
		packet2 := CreateSpAgentConsoleTaskUpd(task)

		task.Message = oldMessage
		task.ClearText = oldText + task.ClearText

		if task.Completed {
			agent.CompletedTasks.Put(task.TaskId, task)
		} else {
			agent.RunningTasks.Put(task.TaskId, task)
		}

		if task.Sync {
			if task.Completed {
				_ = ts.DBMS.DbTaskInsert(task)
			}

			ts.TsSyncAllClients(packet)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet2)
		}

	} else if task.Type == TYPE_TUNNEL {
		var oldMessage string
		if task.Message == "" {
			oldMessage = taskData.Message
		} else {
			oldMessage = task.Message
		}
		oldText := task.ClearText

		task.MessageType = taskData.MessageType
		task.Message = taskData.Message
		task.ClearText = taskData.ClearText

		packet := CreateSpAgentTaskUpdate(task)
		packet2 := CreateSpAgentConsoleTaskUpd(task)

		task.Message = oldMessage
		task.ClearText = oldText + task.ClearText

		if task.Completed {
			agent.CompletedTasks.Put(task.TaskId, task)
		} else {
			agent.RunningTasks.Put(task.TaskId, task)
		}

		if task.Sync {
			if task.Completed {
				_ = ts.DBMS.DbTaskInsert(task)
			}

			ts.TsSyncAllClients(packet)
			ts.TsSyncAllClients(packet2)

			agent.OutConsole.Put(packet2)
			_ = ts.DBMS.DbConsoleInsert(agentId, packet2)
		}

	} else if task.Type == TYPE_TASK || task.Type == TYPE_BROWSER {
		task.MessageType = taskData.MessageType
		task.Message = taskData.Message
		task.ClearText = taskData.ClearText

		if task.Completed {
			agent.CompletedTasks.Put(task.TaskId, task)
		} else {
			agent.RunningTasks.Put(task.TaskId, task)
		}

		if task.Sync {
			if task.Completed {
				_ = ts.DBMS.DbTaskInsert(task)
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

func (ts *Teamserver) TsTaskDelete(agentId string, taskId string) error {
	var (
		agent *Agent
		task  adaptix.TaskData
		value any
		ok    bool
	)

	value, ok = ts.agents.Get(agentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return fmt.Errorf("agent %v not found", agentId)
	}

	for i := 0; i < agent.TasksQueue.Len(); i++ {
		if value, ok = agent.TasksQueue.Get(i); ok {
			task = value.(adaptix.TaskData)
			if task.TaskId == taskId {
				return fmt.Errorf("task %v in process", taskId)
			}
		}
	}

	value, ok = agent.RunningTasks.Get(taskId)
	if ok {
		return fmt.Errorf("task %v in process", taskId)
	}

	value, ok = agent.CompletedTasks.GetDelete(taskId)
	if ok {
		task = value.(adaptix.TaskData)
		_ = ts.DBMS.DbTaskDelete(task.TaskId, "")

		packet := CreateSpAgentTaskRemove(task)
		ts.TsSyncAllClients(packet)
		return nil
	}

	return fmt.Errorf("task %v not found", taskId)
}

/////

func (ts *Teamserver) TsTasksPivotExists(agentId string, first bool) bool {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return false
	}
	agent := value.(*Agent)

	if !first {
		if agent.TasksQueue.Len() > 0 || agent.TunnelQueue.Len() > 0 {
			return true
		}
	}

	for i := 0; i < agent.PivotChilds.Len(); i++ {
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

func (ts *Teamserver) TsTaskQueueGetAvailable(agentId string, availableSize int) ([][]byte, error) {
	var (
		tasksArray [][]byte
		agent      *Agent
		value      any
		ok         bool
	)

	value, ok = ts.agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}
	agent = value.(*Agent)

	/// TASKS QUEUE

	var sendTasks []string

	for i := 0; i < agent.TasksQueue.Len(); i++ {
		value, ok = agent.TasksQueue.Get(i)
		if ok {
			taskData := value.(adaptix.TaskData)
			if len(tasksArray)+len(taskData.Data) < availableSize {
				var taskBuffer bytes.Buffer
				_ = json.NewEncoder(&taskBuffer).Encode(taskData)
				tasksArray = append(tasksArray, taskBuffer.Bytes())
				agent.RunningTasks.Put(taskData.TaskId, taskData)
				agent.TasksQueue.Delete(i)
				i--
				sendTasks = append(sendTasks, taskData.TaskId)
			} else {
				break
			}
		} else {
			break
		}
	}

	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	/// TUNNELS QUEUE

	for i := 0; i < agent.TunnelQueue.Len(); i++ {
		value, ok = agent.TunnelQueue.Get(i)
		if ok {
			tunnelTaskData := value.(adaptix.TaskData)
			if len(tasksArray)+len(tunnelTaskData.Data) < availableSize {
				var taskBuffer bytes.Buffer
				_ = json.NewEncoder(&taskBuffer).Encode(tunnelTaskData)
				tasksArray = append(tasksArray, taskBuffer.Bytes())
				agent.TunnelQueue.Delete(i)
				i--
			} else {
				break
			}
		} else {
			break
		}
	}

	/// PIVOTS QUEUE

	for i := 0; i < agent.PivotChilds.Len(); i++ {
		value, ok = agent.PivotChilds.Get(i)
		if ok {
			lostSize := availableSize - len(tasksArray)
			pivotData := value.(*adaptix.PivotData)
			if availableSize > 0 {
				data, err := ts.TsAgentGetHostedTasks(pivotData.ChildAgentId, lostSize)
				if err != nil {
					continue
				}
				pivotTaskData, err := ts.Extender.ExAgentPivotPackData(agent.Data.Name, pivotData.PivotId, data)
				if err != nil {
					continue
				}
				tasksArray = append(tasksArray, pivotTaskData)
			}
		}
	}

	return tasksArray, nil
}

func (ts *Teamserver) TsTaskStop(agentId string, taskId string) error {
	var (
		agent *Agent
		task  adaptix.TaskData
		value any
		ok    bool
		found bool
	)

	value, ok = ts.agents.Get(agentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return fmt.Errorf("agent %v not found", agentId)
	}

	found = false
	for i := 0; i < agent.TasksQueue.Len(); i++ {
		if value, ok = agent.TasksQueue.Get(i); ok {
			task = value.(adaptix.TaskData)
			if task.TaskId == taskId {
				agent.TasksQueue.Delete(i)
				found = true
				break
			}
		}
	}

	if found {
		packet := CreateSpAgentTaskRemove(task)
		ts.TsSyncAllClients(packet)
		return nil
	}

	value, ok = agent.RunningTasks.Get(taskId)
	if ok {
		task = value.(adaptix.TaskData)
		if task.Type == TYPE_JOB {
			data, err := ts.Extender.ExAgentBrowserJobKill(agent.Data.Name, taskId)
			if err != nil {
				return err
			}

			ts.TsTaskCreate(agent.Data.Id, "job kill "+taskId, "", data)

			return nil
		} else {
			return fmt.Errorf("taski %v in process", taskId)
		}
	}
	return nil
}
