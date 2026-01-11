package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"fmt"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

type TaskHandler interface {
	Create(tm *TaskManager, agent *Agent, taskData *adaptix.TaskData)
	Update(tm *TaskManager, agent *Agent, task *adaptix.TaskData, updateData *adaptix.TaskData)
	PostHook(tm *TaskManager, agent *Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error
	OnClientDisconnect(tm *TaskManager, agent *Agent, task *adaptix.TaskData, clientName string)
}

type TaskManager struct {
	ts       *Teamserver
	handlers map[int]TaskHandler
}

func NewTaskManager(ts *Teamserver) *TaskManager {
	tm := &TaskManager{
		ts:       ts,
		handlers: make(map[int]TaskHandler),
	}

	taskHandler := &TaskTaskHandler{}
	tm.handlers[adaptix.TASK_TYPE_TASK] = taskHandler
	tm.handlers[adaptix.TASK_TYPE_BROWSER] = taskHandler
	tm.handlers[adaptix.TASK_TYPE_JOB] = &JobTaskHandler{}
	tm.handlers[adaptix.TASK_TYPE_TUNNEL] = &TunnelTaskHandler{}

	return tm
}

func (tm *TaskManager) getAgent(agentId string) (*Agent, error) {
	value, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return nil, fmt.Errorf("invalid agent type for %v", agentId)
	}
	return agent, nil
}

func (tm *TaskManager) prepareTaskData(agent *Agent, cmdline string, client string, taskData *adaptix.TaskData) {
	agentData := agent.GetData()

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agentData.Id
	taskData.CommandLine = cmdline
	taskData.Client = client
	taskData.Computer = agentData.Computer
	taskData.StartDate = time.Now().Unix()

	if taskData.Completed {
		taskData.FinishDate = taskData.StartDate
	}

	taskData.User = agentData.Username
	if agentData.Impersonated != "" {
		taskData.User += fmt.Sprintf(" [%s]", agentData.Impersonated)
	}
}

func (tm *TaskManager) syncTaskCreate(agentId string, agent *Agent, taskData *adaptix.TaskData) {
	packet_task := CreateSpAgentTaskSync(*taskData)
	tm.ts.TsSyncAllClients(packet_task)

	packet_console := CreateSpAgentConsoleTaskSync(*taskData)
	tm.ts.TsSyncAllClients(packet_console)

	agent.OutConsole.Put(packet_console)
	_ = tm.ts.DBMS.DbConsoleInsert(agentId, packet_console)
}

func (tm *TaskManager) syncTaskUpdate(agentId string, agent *Agent, taskData *adaptix.TaskData) {

	packet_task := CreateSpAgentTaskUpdate(*taskData)
	if taskData.HandlerId == "" {
		tm.ts.TsSyncAllClients(packet_task)
	} else {
		handlerClient := taskData.Client
		tm.ts.TsSyncExcludeClient(handlerClient, packet_task)
		packet_task.HandlerId = taskData.HandlerId
		tm.ts.TsSyncClient(handlerClient, packet_task)
	}

	packet_console := CreateSpAgentConsoleTaskUpd(*taskData)
	tm.ts.TsSyncAllClients(packet_console)

	agent.OutConsole.Put(packet_console)
	_ = tm.ts.DBMS.DbConsoleInsert(agentId, packet_console)
}

func (tm *TaskManager) completeTask(agent *Agent, taskData *adaptix.TaskData) {
	agent.CompletedTasks.Put(taskData.TaskId, *taskData)
	_ = tm.ts.DBMS.DbTaskInsert(*taskData)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTaskComplete{
		AgentId: taskData.AgentId,
		Task:    *taskData,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskComplete, postEvent)
	// -----------------
}

func (tm *TaskManager) Create(agentId string, cmdline string, client string, taskData adaptix.TaskData) {
	agent, err := tm.getAgent(agentId)
	if err != nil {
		logs.Error("", "TsTaskCreate: %v", err)
		return
	}

	if !agent.Active {
		return
	}

	tm.prepareTaskData(agent, cmdline, client, &taskData)

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTaskCreate{
		AgentId: agentId,
		Task:    taskData,
		Cmdline: cmdline,
		Client:  client,
	}
	if !tm.ts.EventManager.Emit(eventing.EventTaskCreate, eventing.HookPre, preEvent) {
		return
	}
	// ----------------

	handler, ok := tm.handlers[taskData.Type]
	if !ok {
		logs.Debug("", "Unknown task type: %d", taskData.Type)
		return
	}

	handler.Create(tm, agent, &taskData)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTaskCreate{
		AgentId: agentId,
		Task:    taskData,
		Cmdline: cmdline,
		Client:  client,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskCreate, postEvent)
	// -----------------
}

func (tm *TaskManager) Update(agentId string, updateData adaptix.TaskData) {
	agent, err := tm.getAgent(agentId)
	if err != nil {
		logs.Error("", "TsTaskUpdate: %v", err)
		return
	}

	value, ok := agent.RunningTasks.Get(updateData.TaskId)
	if !ok {
		return
	}

	task, ok := value.(adaptix.TaskData)
	if !ok {
		logs.Error("", "TsTaskUpdate: invalid task type for %v", updateData.TaskId)
		return
	}

	task.Data = []byte("")

	handler, ok := tm.handlers[task.Type]
	if !ok {
		logs.Debug("", "Unknown task type: %d", task.Type)
		return
	}

	handler.Update(tm, agent, &task, &updateData)
}

func (tm *TaskManager) PostHook(hookData adaptix.TaskData, jobIndex int) error {
	agent, err := tm.getAgent(hookData.AgentId)
	if err != nil {
		return err
	}

	value, ok := agent.RunningTasks.Get(hookData.TaskId)
	if !ok {
		return fmt.Errorf("task %v not found", hookData.TaskId)
	}

	task, ok := value.(adaptix.TaskData)
	if !ok {
		return fmt.Errorf("invalid task type for %v", hookData.TaskId)
	}

	if task.HookId == "" || task.HookId != hookData.HookId || task.Client != hookData.Client || !tm.ts.TsClientConnected(task.Client) {
		return fmt.Errorf("operation not available")
	}

	handler, ok := tm.handlers[task.Type]
	if !ok {
		return fmt.Errorf("unknown task type: %d", task.Type)
	}

	return handler.PostHook(tm, agent, &task, &hookData, jobIndex)
}

func (tm *TaskManager) Cancel(agentId string, taskId string) error {
	agent, err := tm.getAgent(agentId)
	if err != nil {
		return err
	}

	found, retTask := agent.HostedTasks.RemoveIf(func(v interface{}) bool {
		task, ok := v.(adaptix.TaskData)
		return ok && task.TaskId == taskId
	})

	if found {
		task, ok := retTask.(adaptix.TaskData)
		if ok {
			packet := CreateSpAgentTaskRemove(task)
			tm.ts.TsSyncAllClients(packet)
		}
	}
	return nil
}

func (tm *TaskManager) Delete(agentId string, taskId string) error {
	agent, err := tm.getAgent(agentId)
	if err != nil {
		return err
	}

	found, _ := agent.HostedTasks.FindIf(func(v interface{}) bool {
		task, ok := v.(adaptix.TaskData)
		return ok && task.TaskId == taskId
	})
	if found {
		return fmt.Errorf("task %v in process", taskId)
	}

	_, ok := agent.RunningTasks.Get(taskId)
	if ok {
		return fmt.Errorf("task %v in process", taskId)
	}

	value, ok := agent.CompletedTasks.GetDelete(taskId)
	if !ok {
		return fmt.Errorf("task %v not found", taskId)
	}

	task, ok := value.(adaptix.TaskData)
	if !ok {
		return fmt.Errorf("invalid task type for %v", taskId)
	}

	_ = tm.ts.DBMS.DbTaskDelete(task.TaskId, "")

	packet := CreateSpAgentTaskRemove(task)
	tm.ts.TsSyncAllClients(packet)
	return nil
}

func (tm *TaskManager) Save(taskData adaptix.TaskData) error {
	agent, err := tm.getAgent(taskData.AgentId)
	if err != nil {
		return err
	}

	agentData := agent.GetData()
	taskData.Type = adaptix.TASK_TYPE_TASK
	taskData.TaskId, _ = krypt.GenerateUID(8)
	taskData.Computer = agentData.Computer
	taskData.User = agentData.Username
	taskData.StartDate = time.Now().Unix()
	taskData.FinishDate = taskData.StartDate
	taskData.Sync = true
	taskData.Completed = true

	agent.CompletedTasks.Put(taskData.TaskId, taskData)

	packet_task := CreateSpAgentTaskSync(taskData)
	tm.ts.TsSyncAllClients(packet_task)

	_ = tm.ts.DBMS.DbTaskInsert(taskData)

	return nil
}

func (tm *TaskManager) ProcessDisconnectedClient(clientName string) {
	tm.ts.Agents.ForEach(func(agentId string, agentValue interface{}) bool {
		agent, ok := agentValue.(*Agent)
		if !ok {
			return true
		}

		var tasksToProcess []string
		agent.RunningTasks.ForEach(func(taskId string, taskValue interface{}) bool {
			task, ok := taskValue.(adaptix.TaskData)
			if !ok {
				return true
			}

			if (task.HookId != "" && task.Client == clientName) || (task.Type == adaptix.TASK_TYPE_TUNNEL && task.Client == clientName) {
				tasksToProcess = append(tasksToProcess, taskId)
			}
			return true
		})

		for _, taskId := range tasksToProcess {
			value, ok := agent.RunningTasks.Get(taskId)
			if !ok {
				continue
			}
			task, ok := value.(adaptix.TaskData)
			if !ok {
				continue
			}

			handler, ok := tm.handlers[task.Type]
			if !ok {
				continue
			}

			handler.OnClientDisconnect(tm, agent, &task, clientName)
		}

		return true
	})
}

func (tm *TaskManager) RunningExists(agentId string, taskId string) bool {
	agent, err := tm.getAgent(agentId)
	if err != nil {
		return false
	}
	return agent.RunningTasks.Contains(taskId)
}
