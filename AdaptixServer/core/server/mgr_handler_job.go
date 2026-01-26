package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/safe"

	"github.com/Adaptix-Framework/axc2"
)

const maxAccumulatedSize = 0xa00000 // 10 MB

type JobTaskHandler struct{}

func (h *JobTaskHandler) Create(tm *TaskManager, agent *Agent, taskData *adaptix.TaskData) {
	if taskData.Sync {
		tm.syncTaskCreate(taskData.AgentId, agent, taskData)
	}
	agent.HostedTasks.Push(*taskData)
}

func (h *JobTaskHandler) Update(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	updateData.AgentId = task.AgentId

	// --- EVENT ---
	event := &eventing.EventDataTaskUpdateJob{
		AgentId: task.AgentId,
		Task:    *updateData,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskUpdateJob, event)
	// -------------

	if task.HookId != "" && task.Client != "" && tm.ts.TsClientConnected(task.Client) {
		h.updateWithHook(tm, agent, task, updateData)
		return
	}

	h.updateWithoutHook(tm, agent, task, updateData)
}

func (h *JobTaskHandler) updateWithHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	updateData.HookId = task.HookId

	hookJob := &HookJob{
		Job:       *updateData,
		Processed: false,
		Sent:      false,
	}

	num := 0
	value, ok := agent.RunningJobs.Get(task.TaskId)
	if ok {
		jobs := value.(*safe.Slice)
		jobs.Put(hookJob)
		num = int(jobs.Len() - 1)
	} else {
		jobs := safe.NewSlice()
		jobs.Put(hookJob)
		agent.RunningJobs.Put(task.TaskId, jobs)
	}

	packet := CreateSpAgentTaskHook(*updateData, num)
	tm.ts.TsSyncClient(task.Client, packet)
}

func (h *JobTaskHandler) updateWithoutHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData) {
	if !task.Sync {
		return
	}

	hookJob := &HookJob{
		Job:       *updateData,
		Processed: true,
		Sent:      true,
	}

	value, ok := agent.RunningJobs.Get(task.TaskId)

	if updateData.Completed {
		agent.RunningTasks.Delete(updateData.TaskId)

		if ok {
			jobs := value.(*safe.Slice)
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

		tm.completeTask(agent, task)
	} else {
		if ok {
			jobs := value.(*safe.Slice)
			jobs.Put(hookJob)
		} else {
			jobs := safe.NewSlice()
			jobs.Put(hookJob)
			agent.RunningJobs.Put(task.TaskId, jobs)
		}
	}

	tm.syncTaskUpdate(task.AgentId, agent, updateData)
}

func (h *JobTaskHandler) aggregateJobResults(task *adaptix.TaskData, jobs *safe.Slice) {
	jobsArray := jobs.CutArray()

	for _, jobValue := range jobsArray {
		jobData := jobValue.(*HookJob)

		if task.MessageType != CONSOLE_OUT_ERROR {
			task.MessageType = jobData.Job.MessageType
		}
		if task.Message == "" {
			task.Message = jobData.Job.Message
		}

		if len(task.ClearText)+len(jobData.Job.ClearText) > maxAccumulatedSize {
			remaining := maxAccumulatedSize - len(jobData.Job.ClearText) - 1000
			if remaining > 0 {
				task.ClearText = task.ClearText[len(task.ClearText)-remaining:] + "\n[EARLIER OUTPUT TRUNCATED]\n"
			}
		}
		task.ClearText += jobData.Job.ClearText
	}
}

func (h *JobTaskHandler) PostHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error {
	if !task.Sync {
		return nil
	}

	value, ok := agent.RunningJobs.Get(task.TaskId)
	if !ok {
		return nil
	}
	jobs := value.(*safe.Slice)

	jobValue, ok := jobs.Get(uint(jobIndex))
	if !ok {
		return nil
	}
	jobData := jobValue.(*HookJob)

	jobData.Job.MessageType = hookData.MessageType
	jobData.Job.Message = hookData.Message
	jobData.Job.ClearText = hookData.ClearText
	jobData.Processed = true

	h.processReadyJobs(tm, agent, task, jobs)

	return nil
}

func (h *JobTaskHandler) processReadyJobs(tm *TaskManager, agent *Agent, task *adaptix.TaskData, jobs *safe.Slice) {
	completed := false
	sent := true

	jobs.DirectLock()
	defer jobs.DirectUnlock()

	slice := jobs.DirectSlice()

	for i := 0; i < len(slice); i++ {
		hookJob := slice[i].(*HookJob)
		notProcessBreak := false

		hookJob.mu.Lock()

		if hookJob.Job.Completed {
			completed = true
		}

		if !hookJob.Sent {
			if hookJob.Processed {
				hookJob.Sent = true

				packet_task_update := CreateSpAgentTaskUpdate(hookJob.Job)
				packet_console_update := CreateSpAgentConsoleTaskUpd(hookJob.Job)

				tm.ts.TsSyncAllClientsWithCategory(packet_task_update, SyncCategoryTasksManager)
				tm.ts.TsSyncConsole(packet_console_update, hookJob.Job.Client)

				agent.OutConsole.Put(packet_console_update)
				_ = tm.ts.DBMS.DbConsoleInsert(task.AgentId, packet_console_update)
			} else {
				notProcessBreak = true
			}
		}

		if sent {
			sent = hookJob.Sent
		}

		hookJob.mu.Unlock()

		if notProcessBreak {
			break
		}
	}

	if completed && sent {
		h.finalizeJob(tm, agent, task, slice)
	}
}

func (h *JobTaskHandler) finalizeJob(tm *TaskManager, agent *Agent, task *adaptix.TaskData, slice []interface{}) {
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
	tm.completeTask(agent, task)
}

func (h *JobTaskHandler) OnClientDisconnect(tm *TaskManager, agent *Agent, task *adaptix.TaskData, clientName string) {
	value, ok := agent.RunningJobs.Get(task.TaskId)
	if !ok {
		return
	}
	jobs := value.(*safe.Slice)

	jobs.DirectLock()
	slice := jobs.DirectSlice()

	allProcessed := true
	completed := false

	for i := 0; i < len(slice); i++ {
		hookJob := slice[i].(*HookJob)

		hookJob.mu.Lock()

		if !hookJob.Processed {
			hookJob.Processed = true
		}

		if !hookJob.Sent {
			hookJob.Sent = true

			packet_task_update := CreateSpAgentTaskUpdate(hookJob.Job)
			packet_console_update := CreateSpAgentConsoleTaskUpd(hookJob.Job)

			tm.ts.TsSyncAllClientsWithCategory(packet_task_update, SyncCategoryTasksManager)
			tm.ts.TsSyncConsole(packet_console_update, hookJob.Job.Client)

			agent.OutConsole.Put(packet_console_update)
			_ = tm.ts.DBMS.DbConsoleInsert(task.AgentId, packet_console_update)
		}

		if hookJob.Job.Completed {
			completed = true
		}
		if !hookJob.Sent {
			allProcessed = false
		}

		hookJob.mu.Unlock()
	}

	if completed && allProcessed {
		h.finalizeJob(tm, agent, task, slice)
	}

	jobs.DirectUnlock()
}
