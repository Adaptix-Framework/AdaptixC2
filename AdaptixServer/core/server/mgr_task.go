package server

import (
	"AdaptixServer/core/eventing"
	"bytes"
	"fmt"
	"math/rand/v2"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

type TaskHandler interface {
	Create(tm *TaskManager, agent *adaptix.Agent, taskData *adaptix.TaskData)
	Update(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, updateData *adaptix.TaskData)
	PostHook(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, hookData *adaptix.TaskData, jobIndex int) error
	OnClientDisconnect(tm *TaskManager, agent *adaptix.Agent, task *adaptix.TaskData, clientName string)
}

type TaskManager struct {
	ts          *Teamserver
	handlers    map[int]TaskHandler
	deliverySem chan struct{}
}

func NewTaskManager(ts *Teamserver) *TaskManager {
	tm := &TaskManager{
		ts:          ts,
		handlers:    make(map[int]TaskHandler),
		deliverySem: make(chan struct{}, 100),
	}

	taskHandler := &TaskTaskHandler{}
	tm.handlers[adaptix.TASK_TYPE_TASK] = taskHandler
	tm.handlers[adaptix.TASK_TYPE_BROWSER] = taskHandler
	tm.handlers[adaptix.TASK_TYPE_JOB] = &JobTaskHandler{}
	tm.handlers[adaptix.TASK_TYPE_TUNNEL] = &TunnelTaskHandler{}

	return tm
}

func (tm *TaskManager) prepareTaskData(agent *adaptix.Agent, cmdline string, client string, taskData *adaptix.TaskData) {
	agentData := agent.GetData()

	if taskData.TaskId == 0 {
		if taskData.Sync {
			taskData.TaskId = tm.ts.TsTaskGenID()
		} else {
			taskData.TaskId = int64(rand.Uint64() | 1)
		}
	}
	taskData.AgentId = agentData.Id
	taskData.CommandLine = cmdline
	taskData.Client = client
	taskData.Computer = agentData.Computer
	taskData.StartDate = time.Now().Unix()

	if taskData.Priority < 0 {
		taskData.Priority = 0
	}

	if taskData.Completed {
		taskData.FinishDate = taskData.StartDate
	}

	taskData.User = agentData.Username
	if agentData.Impersonated != "" {
		taskData.User += fmt.Sprintf(" [%s]", agentData.Impersonated)
	}
}

func (tm *TaskManager) syncTaskCreate(agentId int64, taskData *adaptix.TaskData) {
	if taskData.Type != adaptix.TASK_TYPE_BROWSER {
		packet_task := CreateSpAgentTaskSync(*taskData)
		tm.ts.TsSyncAllClientsWithCategory(packet_task, SyncCategoryTasksManager)

		packet_console := CreateSpAgentConsoleTaskSync(*taskData)
		tm.ts.TsSyncConsole(packet_console, taskData.Client, taskData.Client)

		_ = tm.ts.DBMS.DbConsoleInsert(agentId, taskData.Client, packet_console)
	}
}

func (tm *TaskManager) syncTaskUpdate(agentId int64, taskData *adaptix.TaskData) {
	if taskData.Type == adaptix.TASK_TYPE_BROWSER {
		return
	}

	packet_task := CreateSpAgentTaskUpdate(*taskData)
	if taskData.HandlerId == "" {
		tm.ts.TsSyncAllClientsWithCategory(packet_task, SyncCategoryTasksManager)
	} else {
		handlerClient := taskData.Client
		tm.ts.TsSyncExcludeClientWithCategory(handlerClient, packet_task, SyncCategoryTasksManager)
		packet_task.HandlerId = taskData.HandlerId
		tm.ts.TsSyncClient(handlerClient, packet_task)
	}

	packet_console := CreateSpAgentConsoleTaskUpd(*taskData)
	tm.ts.TsSyncConsole(packet_console, taskData.Client, taskData.Client)

	_ = tm.ts.DBMS.DbConsoleInsert(agentId, taskData.Client, packet_console)
}

func (tm *TaskManager) completeTask(taskData *adaptix.TaskData) {
	if taskData.Sync && taskData.Type != adaptix.TASK_TYPE_BROWSER {
		_ = tm.ts.DBMS.DbTaskUpdate(*taskData)
	}

	if taskData.OnComplete != nil {
		tm.safeRunComplete(taskData)
	}

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTaskComplete{
		AgentId: taskData.AgentId,
		Task:    *taskData,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskComplete, postEvent)
	// -----------------
}

func (tm *TaskManager) executeServerHandler(taskData *adaptix.TaskData) {
	if taskData.HandlerId == "" {
		return
	}
	if !tm.ts.TsAxScriptIsServerHook(taskData.HandlerId) {
		return
	}
	handlerData := map[string]interface{}{
		"agent":   taskData.AgentId,
		"task_id": taskData.TaskId,
		"cmdline": taskData.CommandLine,
		"message": taskData.Message,
		"text":    taskData.ClearText,
		"type":    taskData.MessageType,
	}
	_ = tm.ts.TsAxScriptExecHandler(taskData.HandlerId, handlerData, taskData.Client)
	taskData.HandlerId = ""
}

func (tm *TaskManager) Create(agentId int64, cmdline string, client string, taskData adaptix.TaskData) int64 {
	agent, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		tm.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "task_manager", "TsTaskCreate: agent %v not found", agentId)
		return 0
	}

	if !agent.IsActive() {
		return 0
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
		return 0
	}
	// ----------------

	handler, ok := tm.handlers[taskData.Type]
	if !ok {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "Unknown task type: %d", taskData.Type)
		return 0
	}

	if taskData.Sync && taskData.Type != adaptix.TASK_TYPE_BROWSER {
		_ = tm.ts.DBMS.DbTaskInsert(taskData)
	}

	handler.Create(tm, agent, &taskData)

	deliveryFn := tm.ts.TsGetAgentDeliveryFunc(agentId)
	if deliveryFn != nil {
		go func() {
			tm.deliverySem <- struct{}{}
			defer func() { <-tm.deliverySem }()

			err := deliveryFn(agentId, taskData)
			if err != nil {
				tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "delivery callback error for agent %d: %v", agentId, err)
			}
		}()
	}

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTaskCreate{
		AgentId: agentId,
		Task:    taskData,
		Cmdline: cmdline,
		Client:  client,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskCreate, postEvent)
	// -----------------
	return taskData.TaskId
}

func (tm *TaskManager) Update(agentId int64, updateData adaptix.TaskData) {
	agent, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "TsTaskUpdate: agent %v not found (task %v ignored)", agentId, updateData.TaskId)
		return
	}

	task, ok := agent.RunningTasks.Get(updateData.TaskId)
	if !ok {
		return
	}

	task.Data = []byte("")

	if updateData.Client == "" {
		updateData.Client = task.Client
	}
	if updateData.Type == adaptix.TASK_TYPE_TASK && task.Type != adaptix.TASK_TYPE_TASK {
		updateData.Type = task.Type
	}

	handler, ok := tm.handlers[task.Type]
	if !ok {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "Unknown task type: %d", task.Type)
		return
	}

	handler.Update(tm, agent, &task, &updateData)
}

func (tm *TaskManager) PostHook(hookData adaptix.TaskData, jobIndex int) error {
	agent, ok := tm.ts.Agents.Get(hookData.AgentId)
	if !ok {
		return fmt.Errorf("agent %v not found", hookData.AgentId)
	}

	task, ok := agent.RunningTasks.Get(hookData.TaskId)
	if !ok {
		return fmt.Errorf("task %v not found", hookData.TaskId)
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

func (tm *TaskManager) cancelTaskHooks(task adaptix.TaskData) {
	if task.HookId != "" && tm.ts.TsAxScriptIsServerHook(task.HookId) {
		tm.ts.TsAxScriptRemovePostHook(task.HookId)
	}
	if task.HandlerId != "" && tm.ts.TsAxScriptIsServerHook(task.HandlerId) {
		tm.ts.TsAxScriptRemoveHandler(task.HandlerId)
	}
}

func (tm *TaskManager) finalizeCanceled(agent *adaptix.Agent, task adaptix.TaskData) {
	task.Completed = true
	task.MessageType = CONSOLE_OUT_ERROR
	task.Message = "Task canceled"
	task.FinishDate = time.Now().Unix()
	if task.OnComplete != nil {
		tm.safeRunComplete(&task)
	}
	if task.Sync {
		_ = tm.ts.DBMS.DbTaskUpdate(task)
		tm.syncTaskUpdate(agent.GetData().Id, &task)
	} else {
		tm.ts.TsSyncAllClients(CreateSpAgentTaskRemove(task))
	}
}

func (tm *TaskManager) Cancel(agentId int64, taskId int64) error {
	agent, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}

	found, retTask := agent.HostedQueue.RemoveIf(func(v interface{}) bool {
		task, ok := v.(adaptix.TaskData)
		return ok && task.TaskId == taskId
	})

	if found {
		task, ok := retTask.(adaptix.TaskData)
		if ok {
			tm.cancelTaskHooks(task)
			tm.finalizeCanceled(agent, task)
		}
		return nil
	}

	if agent.RunningTasks.Contains(taskId) {
		task, _ := agent.RunningTasks.Get(taskId)
		agent.RunningTasks.Delete(taskId)
		tm.ts.TsFrameResetDownstream(agentId)
		tm.cancelTaskHooks(task)
		tm.finalizeCanceled(agent, task)
		return nil
	}
	return fmt.Errorf("task %d not found", taskId)
}

func (tm *TaskManager) Delete(agentId int64, taskId int64) error {
	agent, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent %v not found", agentId)
	}

	found, _ := agent.HostedQueue.FindIf(func(v interface{}) bool {
		task, ok := v.(adaptix.TaskData)
		return ok && task.TaskId == taskId
	})
	if found {
		return fmt.Errorf("task %v in process", taskId)
	}

	_, ok = agent.RunningTasks.Get(taskId)
	if ok {
		return fmt.Errorf("task %v in process", taskId)
	}

	task, err := tm.ts.DBMS.DbTaskGet(taskId)
	if err != nil {
		return fmt.Errorf("task %v not found", taskId)
	}

	_ = tm.ts.DBMS.DbTaskDelete(task.TaskId, 0)

	packet := CreateSpAgentTaskRemove(task)
	tm.ts.TsSyncAllClients(packet)
	return nil
}

func (tm *TaskManager) Save(taskData adaptix.TaskData) error {
	agent, ok := tm.ts.Agents.Get(taskData.AgentId)
	if !ok {
		return fmt.Errorf("agent %v not found", taskData.AgentId)
	}

	agentData := agent.GetData()
	taskData.Type = adaptix.TASK_TYPE_TASK
	taskData.TaskId = tm.ts.IdGen.Next("task")
	taskData.Computer = agentData.Computer
	taskData.User = agentData.Username
	taskData.StartDate = time.Now().Unix()
	taskData.FinishDate = taskData.StartDate
	taskData.Sync = true
	taskData.Completed = true

	packet_task := CreateSpAgentTaskSync(taskData)
	tm.ts.TsSyncAllClients(packet_task)

	_ = tm.ts.DBMS.DbTaskInsert(taskData)

	return nil
}

func (tm *TaskManager) ProcessDisconnectedClient(clientName string) {
	agents := make([]*adaptix.Agent, 0, tm.ts.Agents.Len())
	tm.ts.Agents.ForEachFast(func(_ int64, agent *adaptix.Agent) bool {
		agents = append(agents, agent)
		return true
	})

	for _, agent := range agents {
		var tasksToProcess []int64
		agent.RunningTasks.ForEachFast(func(taskId int64, task adaptix.TaskData) bool {

			if task.HookId != "" && task.Client == clientName {
				tasksToProcess = append(tasksToProcess, taskId)
				return true
			}

			if task.Type == adaptix.TASK_TYPE_TUNNEL && task.Client == clientName {
				clientHostedTunnel := false
				tm.ts.TunnelManager.ForEachTunnel(func(_ int64, tunnel *Tunnel) bool {
					if tunnel != nil && tunnel.TaskId == task.TaskId && tunnel.Data.Client == clientName {
						clientHostedTunnel = true
						return false
					}
					return true
				})

				if clientHostedTunnel {
					tasksToProcess = append(tasksToProcess, taskId)
				}
			}
			return true
		})

		for _, taskId := range tasksToProcess {
			task, ok := agent.RunningTasks.Get(taskId)
			if !ok {
				continue
			}

			handler, ok := tm.handlers[task.Type]
			if !ok {
				continue
			}

			handler.OnClientDisconnect(tm, agent, &task, clientName)
		}
	}
}

func (tm *TaskManager) RunningExists(agentId int64, taskId int64) bool {
	agent, ok := tm.ts.Agents.Get(agentId)
	if !ok {
		return false
	}
	return agent.RunningTasks.Contains(taskId)
}

func (tm *TaskManager) DispatchPreEncode(agent *adaptix.Agent, tasks []adaptix.TaskData) {
	for i := range tasks {
		t := &tasks[i]
		if t.OnDispatch != nil {
			tm.safeRunDispatch(t)

			if t.Completed {
				switch t.Type {
				case adaptix.TASK_TYPE_JOB:
					tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "OnDispatch set Completed=true on a JOB task %d — ignored at finalize", t.TaskId)
				case adaptix.TASK_TYPE_BROWSER:
					tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "OnDispatch set Completed=true on a BROWSER task %d — ignored at finalize", t.TaskId)
				}
			}
		}
	}
}

func (tm *TaskManager) DispatchPostEncode(agent *adaptix.Agent, tasks []adaptix.TaskData) {
	for i := range tasks {
		t := &tasks[i]

		switch {
		case t.Completed && t.Sync && t.Type != adaptix.TASK_TYPE_JOB && t.Type != adaptix.TASK_TYPE_BROWSER:
			tm.finalizeAtDispatch(agent, t)

		case t.Repeat && t.OnDispatch != nil && !t.Completed:
			cont := *t
			cont.Data = nil
			cont.DispatchBudget = 0
			agent.HostedQueue.Push(cont.Priority, cont)

		case t.Sync || t.Type == adaptix.TASK_TYPE_BROWSER:
			agent.RunningTasks.Put(t.TaskId, *t)
			if t.Sync {
				_ = tm.ts.DBMS.DbTaskMarkDispatched(t.TaskId)
			}
		}
	}
}

const onDispatchSlowThreshold = 100 * time.Millisecond

func (tm *TaskManager) safeRunDispatch(t *adaptix.TaskData) {
	defer func() {
		if r := recover(); r != nil {
			tm.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "task_manager", "OnDispatch panic for task %d: %v", t.TaskId, r)
			t.Completed = true
		}
	}()

	var (
		auditDispatch = tm.ts.LogManager != nil && tm.ts.LogManager.IsDebug()
		before        adaptix.TaskData
	)
	if auditDispatch {
		before = *t
	}

	start := time.Now()
	t.OnDispatch(tm.ts, t)
	if elapsed := time.Since(start); elapsed > onDispatchSlowThreshold {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "slow OnDispatch task=%d agent=%d took=%v",
			t.TaskId, t.AgentId, elapsed)
	}

	if auditDispatch {
		if diff := taskDispatchDiff(&before, t); diff != "" {
			tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager",
				"OnDispatch mutated task %d: %s", t.TaskId, diff)
		}
	}
}

func taskDispatchDiff(a, b *adaptix.TaskData) string {
	var parts []string
	if a.Completed != b.Completed {
		parts = append(parts, fmt.Sprintf("Completed: %v→%v", a.Completed, b.Completed))
	}
	if a.FinishDate != b.FinishDate {
		parts = append(parts, fmt.Sprintf("FinishDate: %d→%d", a.FinishDate, b.FinishDate))
	}
	if a.MessageType != b.MessageType {
		parts = append(parts, fmt.Sprintf("MessageType: %d→%d", a.MessageType, b.MessageType))
	}
	if a.Message != b.Message {
		parts = append(parts, fmt.Sprintf("Message: %q→%q", a.Message, b.Message))
	}
	if a.ClearText != b.ClearText {
		parts = append(parts, fmt.Sprintf("ClearText: %q→%q", a.ClearText, b.ClearText))
	}
	if !bytes.Equal(a.Data, b.Data) {
		parts = append(parts, fmt.Sprintf("Data: %d→%d bytes", len(a.Data), len(b.Data)))
	}
	if a.HookId != b.HookId {
		parts = append(parts, fmt.Sprintf("HookId: %q→%q", a.HookId, b.HookId))
	}
	if a.HandlerId != b.HandlerId {
		parts = append(parts, fmt.Sprintf("HandlerId: %q→%q", a.HandlerId, b.HandlerId))
	}
	return strings.Join(parts, ", ")
}

func (tm *TaskManager) finalizeAtDispatch(agent *adaptix.Agent, t *adaptix.TaskData) {
	if t.FinishDate == 0 {
		t.FinishDate = time.Now().Unix()
	}
	if t.MessageType == 0 {
		t.MessageType = CONSOLE_OUT_SUCCESS
	}

	_ = tm.ts.DBMS.DbTaskUpdate(*t)
	_ = tm.ts.DBMS.DbTaskMarkDispatched(t.TaskId)

	upd := CreateSpAgentTaskUpdate(*t)
	tm.ts.TsSyncAllClientsWithCategory(upd, SyncCategoryTasksManager)

	cu := CreateSpAgentConsoleTaskUpd(*t)
	tm.ts.TsSyncConsole(cu, t.Client, t.Client)
	_ = tm.ts.DBMS.DbConsoleInsert(agent.GetData().Id, t.Client, cu)

	if t.OnComplete != nil {
		tm.safeRunComplete(t)
	}

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTaskComplete{
		AgentId: t.AgentId,
		Task:    *t,
	}
	tm.ts.EventManager.EmitAsync(eventing.EventTaskComplete, postEvent)
	// -----------------
}

const onCompleteSlowThreshold = 100 * time.Millisecond

func (tm *TaskManager) safeRunComplete(t *adaptix.TaskData) {
	defer func() {
		if r := recover(); r != nil {
			tm.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "task_manager", "OnComplete panic for task %d: %v", t.TaskId, r)
		}
	}()

	start := time.Now()
	t.OnComplete(tm.ts, t)
	if elapsed := time.Since(start); elapsed > onCompleteSlowThreshold {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "task_manager", "slow OnComplete task=%d agent=%d took=%v", t.TaskId, t.AgentId, elapsed)
	}
}
