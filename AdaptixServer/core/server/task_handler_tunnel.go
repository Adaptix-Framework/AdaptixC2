package server

import (
	"time"

	"github.com/Adaptix-Framework/axc2"
)

type TunnelTaskHandler struct{}

func (h *TunnelTaskHandler) Create(tm *TaskManager, agent *Agent, taskData *adaptix.TaskData) {
	if taskData.Sync {
		if taskData.Completed {
			agent.CompletedTasks.Put(taskData.TaskId, *taskData)
		} else {
			agent.RunningTasks.Put(taskData.TaskId, *taskData)
		}

		tm.syncTaskCreate(taskData.AgentId, agent, taskData)

		if taskData.Completed {
			_ = tm.ts.DBMS.DbTaskInsert(*taskData)
		}
	}
}

func (h *TunnelTaskHandler) Update(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
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
			tm.completeTask(agent, task)
		} else {
			agent.RunningTasks.Put(task.TaskId, *task)
		}

		tm.syncTaskUpdate(task.AgentId, agent, &tmpTask)
	}
}

func (h *TunnelTaskHandler) PostHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error {
	return nil
}

func (h *TunnelTaskHandler) OnClientDisconnect(tm *TaskManager, agent *Agent, task *adaptix.TaskData, clientName string) {
	agent.RunningTasks.Delete(task.TaskId)

	task.Completed = true
	task.FinishDate = time.Now().Unix()
	task.MessageType = CONSOLE_OUT_INFO
	task.Message = "Tunnel closed (client disconnected)"

	if task.Sync {
		tm.completeTask(agent, task)
		tm.syncTaskUpdate(task.AgentId, agent, task)
	}
}
