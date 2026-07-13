package server

import (
	"AdaptixServer/core/eventing"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/Adaptix-Framework/axsafe"
)

const maxAccumulatedSize = 0xa00000 // 10 MB

type JobTaskHandler struct{}

func (h *JobTaskHandler) Create(tm *TaskManager, agent *adaptix.Agent, taskData *adaptix.TaskData) {
	if taskData.Sync {
		tm.syncTaskCreate(taskData.AgentId, taskData)
	}
	agent.HostedQueue.Push(taskData.Priority, *taskData)
}

func (h *JobTaskHandler) Update(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	updateData.AgentId = task.AgentId

	// --- EVENT ---
	event := &eventing.EventDataTaskUpdateJob{
		AgentId: task.AgentId,
		Task:    *updateData,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskUpdateJob, event)
	// -------------

	/// Server-side hook: execute directly
	if task.HookId != "" && tm.ts.TsAxScriptIsServerHook(task.HookId) {
		h.updateWithServerHook(tm, agent, task, updateData)
		return
	}

	/// Client-side hook: send to client
	if task.HookId != "" && task.Client != "" && tm.ts.TsClientConnected(task.Client) {
		h.updateWithHook(tm, agent, task, updateData)
		return
	}

	h.updateWithoutHook(tm, agent, task, updateData)
}

func (h *JobTaskHandler) updateWithServerHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	hookData := map[string]interface{}{
		"agent":     task.AgentId,
		"task_id":   task.TaskId,
		"message":   updateData.Message,
		"text":      updateData.ClearText,
		"type":      updateData.MessageType,
		"completed": updateData.Completed,
	}
	result, _ := tm.ts.TsAxScriptExecPostHook(task.HookId, hookData, task.Client)
	if result != nil {
		if msg, ok := result["message"].(string); ok {
			updateData.Message = msg
		}
		if txt, ok := result["text"].(string); ok {
			updateData.ClearText = txt
		}
		if mt, ok := result["type"].(int); ok {
			updateData.MessageType = mt
		}
	}

	if updateData.Completed {
		tm.ts.TsAxScriptRemovePostHook(task.HookId)
	}

	h.updateWithoutHook(tm, agent, task, updateData)
}

func (h *JobTaskHandler) updateWithHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	updateData.HookId = task.HookId

	hookJob := &adaptix.HookJob{
		Job:       *updateData,
		Processed: false,
		Sent:      false,
	}

	num := 0
	jobs, ok := agent.RunningJobs.Get(task.TaskId)
	if ok {
		jobs.Put(hookJob)
		num = int(jobs.Len() - 1)
	} else {
		jobs := axsafe.NewSlice()
		jobs.Put(hookJob)
		agent.RunningJobs.Put(task.TaskId, jobs)
	}

	packet := CreateSpAgentTaskHook(*updateData, num)
	tm.ts.TsSyncClient(task.Client, packet)
}

func (h *JobTaskHandler) updateWithoutHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	if !task.Sync {
		return
	}

	hookJob := &adaptix.HookJob{
		Job:       *updateData,
		Processed: true,
		Sent:      true,
	}

	jobs, ok := agent.RunningJobs.Get(task.TaskId)

	if updateData.Completed {
		agent.RunningTasks.Delete(updateData.TaskId)

		if ok {
			jobs.Put(hookJob)
			h.aggregateJobResults(task, jobs)
			agent.RunningJobs.Delete(task.TaskId)
		} else {
			task.MessageType = updateData.MessageType
			task.Message = updateData.Message
			task.ClearText = updateData.ClearText
		}

		task.FinishDate = updateData.FinishDate
		task.Completed = updateData.Completed

		updateData.HandlerId = task.HandlerId

		tm.completeTask(task)
	} else {
		if ok {
			jobs.Put(hookJob)
		} else {
			newJobs := axsafe.NewSlice()
			newJobs.Put(hookJob)
			agent.RunningJobs.Put(task.TaskId, newJobs)
		}
	}

	tm.syncTaskUpdate(task.AgentId, updateData)
}

func (h *JobTaskHandler) aggregateJobResults(task *adaptix.TaskData, jobs *axsafe.Slice) {
	jobsArray := jobs.CutArray()

	for _, jobValue := range jobsArray {
		jobData := jobValue.(*adaptix.HookJob)

		if task.MessageType != CONSOLE_OUT_ERROR {
			task.MessageType = jobData.Job.MessageType
		}
		if task.Message == "" {
			task.Message = jobData.Job.Message
		}

		if len(task.ClearText)+len(jobData.Job.ClearText) > maxAccumulatedSize {
			remaining := maxAccumulatedSize - len(jobData.Job.ClearText) - 1000
			if remaining <= 0 {
				task.ClearText = ""
			} else {
				task.ClearText = task.ClearText[len(task.ClearText)-remaining:] + "\n[EARLIER OUTPUT TRUNCATED]\n"
			}
		}
		task.ClearText += jobData.Job.ClearText
	}
}

func (h *JobTaskHandler) PostHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error {
	if !task.Sync {
		return nil
	}

	jobs, ok := agent.RunningJobs.Get(task.TaskId)
	if !ok {
		return nil
	}

	jobValue, ok := jobs.Get(uint(jobIndex))
	if !ok {
		return nil
	}
	jobData := jobValue.(*adaptix.HookJob)

	jobData.Mu.Lock()
	jobData.Job.MessageType = hookData.MessageType
	jobData.Job.Message = hookData.Message
	jobData.Job.ClearText = hookData.ClearText
	jobData.Processed = true
	jobData.Mu.Unlock()

	h.processReadyJobs(tm, agent, task, jobs)

	return nil
}

func (h *JobTaskHandler) processReadyJobs(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, jobs *axsafe.Slice) {
	completed := false
	sent := true

	jobs.DirectLock()

	slice := jobs.DirectSlice()

	for i := 0; i < len(slice); i++ {
		hookJob := slice[i].(*adaptix.HookJob)
		notProcessBreak := false

		hookJob.Mu.Lock()

		if hookJob.Job.Completed {
			completed = true
		}

		if !hookJob.Sent {
			if hookJob.Processed {
				hookJob.Sent = true

				packet_task_update := CreateSpAgentTaskUpdate(hookJob.Job)
				packet_console_update := CreateSpAgentConsoleTaskUpd(hookJob.Job)

				tm.ts.TsSyncAllClientsWithCategory(packet_task_update, SyncCategoryTasksManager)
				tm.ts.TsSyncConsole(packet_console_update, hookJob.Job.Client, hookJob.Job.Client)

				_ = tm.ts.DBMS.DbConsoleInsert(task.AgentId, hookJob.Job.Client, packet_console_update)
			} else {
				notProcessBreak = true
			}
		}

		if sent {
			sent = hookJob.Sent
		}

		hookJob.Mu.Unlock()

		if notProcessBreak {
			break
		}
	}

	if completed && sent {
		if task.Completed {
			jobs.DirectUnlock()
			return
		}
		task.Completed = true
		snapshot := make([]interface{}, len(slice))
		copy(snapshot, slice)
		jobs.DirectUnlock()
		h.finalizeJob(tm, agent, task, snapshot)
		return
	}

	jobs.DirectUnlock()
}

func (h *JobTaskHandler) finalizeJob(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, slice []interface{}) {
	agent.RunningTasks.Delete(task.TaskId)

	for i := 0; i < len(slice); i++ {
		hookJob := slice[i].(*adaptix.HookJob)

		hookJob.Mu.RLock()
		jobData := hookJob.Job
		hookJob.Mu.RUnlock()

		if task.MessageType != CONSOLE_OUT_ERROR {
			task.MessageType = jobData.MessageType
		}
		if task.Message == "" {
			task.Message = jobData.Message
		}

		if len(task.ClearText)+len(jobData.ClearText) > maxAccumulatedSize {
			remaining := maxAccumulatedSize - len(jobData.ClearText) - 1000
			if remaining <= 0 {
				task.ClearText = ""
			} else {
				task.ClearText = task.ClearText[len(task.ClearText)-remaining:] + "\n[EARLIER OUTPUT TRUNCATED]\n"
			}
		}
		task.ClearText += jobData.ClearText

		task.FinishDate = jobData.FinishDate
		task.Completed = jobData.Completed
	}

	agent.RunningJobs.Delete(task.TaskId)
	tm.completeTask(task)
	tm.executeServerHandler(task)
}

func (h *JobTaskHandler) OnClientDisconnect(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, clientName string) {
	jobs, ok := agent.RunningJobs.Get(task.TaskId)
	if !ok {
		return
	}

	jobs.DirectLock()
	slice := jobs.DirectSlice()

	allProcessed := true

	if task.Completed {
		jobs.DirectUnlock()
		return
	}

	completed := false

	for i := 0; i < len(slice); i++ {
		hookJob := slice[i].(*adaptix.HookJob)

		hookJob.Mu.Lock()

		if !hookJob.Processed {
			hookJob.Processed = true
		}

		if !hookJob.Sent {
			hookJob.Sent = true

			packet_task_update := CreateSpAgentTaskUpdate(hookJob.Job)
			packet_console_update := CreateSpAgentConsoleTaskUpd(hookJob.Job)

			tm.ts.TsSyncAllClientsWithCategory(packet_task_update, SyncCategoryTasksManager)
			tm.ts.TsSyncConsole(packet_console_update, hookJob.Job.Client, hookJob.Job.Client)

			_ = tm.ts.DBMS.DbConsoleInsert(task.AgentId, hookJob.Job.Client, packet_console_update)
		}

		if hookJob.Job.Completed {
			completed = true
		}
		if !hookJob.Sent {
			allProcessed = false
		}

		hookJob.Mu.Unlock()
	}

	if completed && allProcessed {
		if task.Completed {
			jobs.DirectUnlock()
			return
		}
		task.Completed = true
		snapshot := make([]interface{}, len(slice))
		copy(snapshot, slice)
		jobs.DirectUnlock()
		h.finalizeJob(tm, agent, task, snapshot)
		return
	}

	jobs.DirectUnlock()
}
