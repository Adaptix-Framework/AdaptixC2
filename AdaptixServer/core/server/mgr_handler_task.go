package server

import (
	"github.com/Adaptix-Framework/axc2"
)

type TaskTaskHandler struct{}

func (h *TaskTaskHandler) Create(tm *TaskManager, agent *Agent, taskData *adaptix.TaskData) {
	if taskData.Sync {
		tm.syncTaskCreate(taskData.AgentId, agent, taskData)
	}
	agent.HostedTasks.Push(*taskData)
}

func (h *TaskTaskHandler) Update(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	agent.RunningTasks.Delete(updateData.TaskId)

	task.FinishDate = updateData.FinishDate
	task.Completed = updateData.Completed
	task.MessageType = updateData.MessageType
	task.Message = updateData.Message
	task.ClearText = updateData.ClearText

	if task.HookId != "" && task.Client != "" && tm.ts.TsClientConnected(task.Client) {
		agent.RunningTasks.Put(task.TaskId, *task)

		packet := CreateSpAgentTaskHook(*task, 0)
		tm.ts.TsSyncClient(task.Client, packet)
		return
	}

	if task.Sync {
		if task.Completed {
			tm.completeTask(agent, task)
			updateData.HandlerId = task.HandlerId
		} else {
			agent.RunningTasks.Put(task.TaskId, *task)
		}

		tm.syncTaskUpdate(task.AgentId, agent, task)
	}
}

func (h *TaskTaskHandler) PostHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error {
	_, ok := agent.RunningTasks.GetDelete(hookData.TaskId)
	if !ok {
		return nil
	}

	task.MessageType = hookData.MessageType
	task.Message = hookData.Message
	task.ClearText = hookData.ClearText

	if task.Sync {
		if task.Completed {
			task.HookId = ""
			tm.completeTask(agent, task)
		} else {
			agent.RunningTasks.Put(task.TaskId, *task)
		}

		tm.syncTaskUpdate(task.AgentId, agent, task)
	}

	return nil
}

func (h *TaskTaskHandler) OnClientDisconnect(tm *TaskManager, agent *Agent, task *adaptix.TaskData, clientName string) {
	if !task.Sync {
		return
	}

	if task.Completed {
		task.HookId = ""
		agent.RunningTasks.Delete(task.TaskId)
		tm.completeTask(agent, task)
	}

	tm.syncTaskUpdate(task.AgentId, agent, task)
}
