package server

import (
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

type TunnelTaskHandler struct{}

func (h *TunnelTaskHandler) Create(tm *TaskManager, agent *adaptix.Agent, taskData *adaptix.TaskData) {
	if taskData.Sync {
		if !taskData.Completed {
			agent.RunningTasks.Put(taskData.TaskId, *taskData)
		}

		tm.syncTaskCreate(taskData.AgentId, taskData)
	}
}

func (h *TunnelTaskHandler) Update(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	agent.RunningTasks.Delete(updateData.TaskId)

	task.FinishDate = updateData.FinishDate
	task.Completed = updateData.Completed
	task.MessageType = updateData.MessageType

	tmpTask := *task
	tmpTask.Message = updateData.Message
	tmpTask.ClearText = updateData.ClearText

	if task.Message == "" {
		task.Message = updateData.Message
	}
	task.ClearText += updateData.ClearText

	if task.Sync {
		if task.Completed {
			tm.completeTask(task)
			updateData.HandlerId = task.HandlerId
		} else {
			agent.RunningTasks.Put(task.TaskId, *task)
		}

		tm.syncTaskUpdate(task.AgentId, &tmpTask)
	}
}

func (h *TunnelTaskHandler) PostHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error {
	return nil
}

func (h *TunnelTaskHandler) OnClientDisconnect(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, clientName string) {
	var tunnelIds []int64
	tm.ts.TunnelManager.ForEachTunnel(func(id int64, tunnel *Tunnel) bool {
		if tunnel != nil && tunnel.TaskId == task.TaskId {
			tunnelIds = append(tunnelIds, id)
		}
		return true
	})
	for _, id := range tunnelIds {
		_ = tm.ts.TsTunnelStop(id)
	}

	if _, ok := agent.RunningTasks.Get(task.TaskId); !ok {
		return
	}

	agent.RunningTasks.Delete(task.TaskId)

	task.Completed = true
	task.FinishDate = time.Now().Unix()
	task.MessageType = CONSOLE_OUT_INFO
	task.Message = "Tunnel closed (client disconnected)"

	if task.Sync {
		tm.completeTask(task)
		tm.syncTaskUpdate(task.AgentId, task)
	}
}
