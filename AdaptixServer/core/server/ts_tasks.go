package server

import (
	"AdaptixServer/core/utils/logs"
	"bytes"
	"encoding/json"
	"fmt"
)

func (ts *Teamserver) TsTaskQueueAddQuite(agentId string, taskObject []byte) {
	var (
		agent    *Agent
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)
	err = json.Unmarshal(taskObject, &taskData)
	if err != nil {
		return
	}

	taskData.Sync = false

	value, ok = ts.agents.Get(agentId)
	if ok {
		agent = value.(*Agent)
	} else {
		logs.Error("TsTaskQueueAdd: agent %v not found", agentId)
		return
	}

	agent.TasksQueue.Put(taskData)
}

func (ts *Teamserver) TsTaskQueueGetAvailable(agentId string, availableSize int) ([][]byte, error) {
	var (
		tasksArray [][]byte
		agent      *Agent
		task       adaptix.TaskData
		value      any
		ok         bool
	)

	value, ok = ts.agents.Get(agentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: agent %v not found", agentId)
	}

	for i := 0; i < agent.TasksQueue.Len(); i++ {
		value, ok = agent.TasksQueue.Get(i)
		if ok {
			task = value.(adaptix.TaskData)
			if len(tasksArray)+len(task.Data) < availableSize {
				var taskBuffer bytes.Buffer
				_ = json.NewEncoder(&taskBuffer).Encode(task)
				tasksArray = append(tasksArray, taskBuffer.Bytes())
				agent.Tasks.Put(task.TaskId, task)
				agent.TasksQueue.Delete(i)
				i--
			} else {
				break
			}
		} else {
			break
		}
	}
	return tasksArray, nil
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
	if ok {
		agent = value.(*Agent)
	} else {
		logs.Error("TsTaskUpdate: agent %v not found", agentId)
		return
	}

	value, ok = agent.Tasks.GetDelete(taskData.TaskId)
	if !ok {
		return
	}

	task = value.(adaptix.TaskData)
	task.Data = []byte("")
	task.FinishDate = taskData.FinishDate
	task.MessageType = taskData.MessageType
	task.Message = taskData.Message
	task.ClearText = taskData.ClearText
	task.Completed = taskData.Completed

	if task.Completed {
		agent.ClosedTasks.Put(task.TaskId, task)
	} else {
		agent.Tasks.Put(task.TaskId, task)
	}

	if task.Sync {
		packet := CreateSpAgentTaskUpdate(task)
		ts.TsSyncAllClients(packet)
		ts.TsSyncSavePacket(packet.store, packet)
	}
}
