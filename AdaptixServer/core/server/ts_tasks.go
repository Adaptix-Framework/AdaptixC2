package server

import (
	"encoding/json"
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsTaskGenID() int64 {
	return ts.IdGen.Next("task")
}

func (ts *Teamserver) TsTaskRunningExists(agentId int64, taskId int64) bool {
	return ts.TaskManager.RunningExists(agentId, taskId)
}

func (ts *Teamserver) TsTaskCreate(agentId int64, cmdline string, client string, taskData adaptix.TaskData) {
	ts.TaskManager.Create(agentId, cmdline, client, taskData)
}

func (ts *Teamserver) TsTaskUpdate(agentId int64, updateData adaptix.TaskData) {
	ts.TaskManager.Update(agentId, updateData)
}

func (ts *Teamserver) TsTaskPostHook(hookData adaptix.TaskData, jobIndex int) error {
	return ts.TaskManager.PostHook(hookData, jobIndex)
}

func (ts *Teamserver) TsTaskCancel(agentId int64, taskId int64) error {
	return ts.TaskManager.Cancel(agentId, taskId)
}

func (ts *Teamserver) TsTaskDelete(agentId int64, taskId int64) error {
	return ts.TaskManager.Delete(agentId, taskId)
}

func (ts *Teamserver) TsTaskSave(taskData adaptix.TaskData) error {
	return ts.TaskManager.Save(taskData)
}

///// Get Tasks

func (ts *Teamserver) extractTasks(agent *adaptix.Agent, maxSize int, maxCount int, priority *uint) (tasks []adaptix.TaskData, sendTasks []int64, usedSize int) {
	count := 0
	for {
		if maxCount > 0 && count >= maxCount {
			break
		}

		var (
			item interface{}
			err  error
		)
		if priority != nil {
			item, err = agent.HostedQueue.PopByPriority(*priority)
		} else {
			item, err = agent.HostedQueue.Pop()
		}

		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if maxSize > 0 && usedSize+len(taskData.Data) >= maxSize {
			agent.HostedQueue.Push(taskData.Priority, taskData)
			break
		}

		if taskData.Repeat && maxSize > 0 {
			taskData.DispatchBudget = maxSize - usedSize
		}

		tasks = append(tasks, taskData)
		if taskData.Type != adaptix.TASK_TYPE_TUNNEL && taskData.Type != adaptix.TASK_TYPE_PROXY_DATA {
			sendTasks = append(sendTasks, taskData.TaskId)
		}
		usedSize += len(taskData.Data)
		count++
	}
	return
}

func (ts *Teamserver) extractPivotTasks(agent *adaptix.Agent, availableSize int, startSize int) (tasks []adaptix.TaskData, usedSize int) {
	usedSize = startSize
	for i := uint(0); i < agent.PivotChilds.Len(); i++ {
		value, ok := agent.PivotChilds.Get(i)
		if !ok {
			break
		}
		pivotData := value.(*adaptix.PivotData)
		lostSize := availableSize - usedSize
		if lostSize <= 0 {
			break
		}
		data, _, err := ts.TsAgentGetHostedAll(pivotData.ChildAgentId, lostSize)
		if err != nil {
			continue
		}
		pivotTaskData, err := agent.Fn.PivotPackData(pivotData.PivotId, data)
		if err != nil {
			continue
		}
		tasks = append(tasks, pivotTaskData)
		usedSize += len(pivotTaskData.Data)
	}
	return
}

func (ts *Teamserver) syncSendTasks(sendTasks []int64) {
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}
}

func (ts *Teamserver) TsTaskGetAvailableAll(agentId int64, availableSize int) ([]adaptix.TaskData, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}

	hostedTasks, sendTasks, size := ts.extractTasks(agent, availableSize, -1, nil)
	ts.syncSendTasks(sendTasks)

	pivotTasks, _ := ts.extractPivotTasks(agent, availableSize, size)

	return append(hostedTasks, pivotTasks...), nil
}

func (ts *Teamserver) TsTaskGetAvailableTasks(agentId int64, maxCount int, availableSize int) ([]adaptix.TaskData, int, error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, 0, fmt.Errorf("agent %v not found", agentId)
	}

	tasks, sendTasks, size := ts.extractTasks(agent, availableSize, maxCount, nil)
	ts.syncSendTasks(sendTasks)

	return tasks, size, nil
}

/// Get Pivot Tasks

func (ts *Teamserver) TsTasksPivotExists(agentId int64, first bool) bool {
	return ts.tsTasksPivotExistsWithVisited(agentId, first, make(map[int64]bool))
}

func (ts *Teamserver) tsTasksPivotExistsWithVisited(agentId int64, first bool, visited map[int64]bool) bool {
	if !ts.TsAgentIsExists(agentId) {
		return false
	}

	if visited[agentId] {
		return false
	}
	visited[agentId] = true

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return false
	}

	if !first {
		if agent.HostedQueue.Len() > 0 {
			return true
		}
	}

	for i := uint(0); i < agent.PivotChilds.Len(); i++ {
		value, ok := agent.PivotChilds.Get(i)
		if ok {
			pivotData := value.(*adaptix.PivotData)
			if ts.tsTasksPivotExistsWithVisited(pivotData.ChildAgentId, false, visited) {
				return true
			}
		}
	}
	return false
}

func (ts *Teamserver) TsProcessHookJobsForDisconnectedClient(clientName string) {
	ts.TaskManager.ProcessDisconnectedClient(clientName)
}

type TaskListItem struct {
	TaskType   int    `json:"a_task_type"`
	TaskId     int64  `json:"a_task_id"`
	AgentId    int64  `json:"a_id"`
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

type TaskPage struct {
	Items  []TaskListItem `json:"items"`
	Total  int            `json:"total"`
	Offset int            `json:"offset"`
	Limit  int            `json:"limit"`
}

func (ts *Teamserver) TsTasksGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string, completedFilter *bool) ([]byte, error) {
	if agentId != 0 && !ts.TsAgentIsExists(agentId) {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}

	tasks, total, err := ts.DBMS.DbTasksGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder, completedFilter)
	if err != nil {
		return nil, err
	}

	items := make([]TaskListItem, 0, len(tasks))
	for _, task := range tasks {
		overlayLiveJobText(ts, &task)
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
			Completed:  task.Completed,
		})
	}

	return json.Marshal(TaskPage{
		Items:  items,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

type ConsolePage struct {
	Items    []json.RawMessage `json:"items"`
	Total    int               `json:"total"`
	OldestId int64             `json:"oldest_id"`
	HasMore  bool              `json:"has_more"`
}

func (ts *Teamserver) TsConsoleGetPage(agentId int64, afterId int64, limit int, username string) ([]byte, error) {
	teamMode := true
	if client, ok := ts.Broker.GetClient(username); ok {
		teamMode = client.ConsoleTeamMode()
	}

	raw, total, oldestId, err := ts.DBMS.DbConsoleGetPage(agentId, afterId, limit, username, teamMode)
	if err != nil {
		return nil, err
	}

	items := make([]json.RawMessage, 0, len(raw))
	for _, message := range raw {
		items = append(items, json.RawMessage(message))
	}

	return json.Marshal(ConsolePage{
		Items:    items,
		Total:    total,
		OldestId: oldestId,
		HasMore:  len(items) == limit,
	})
}

type ConsoleSearchHitItem struct {
	Id      int64           `json:"id"`
	Snippet string          `json:"snippet"`
	Packet  json.RawMessage `json:"packet"`
}

type ConsoleSearchPage struct {
	Items  []ConsoleSearchHitItem `json:"items"`
	Total  int                    `json:"total"`
	Limit  int                    `json:"limit"`
	Offset int                    `json:"offset"`
}

func (ts *Teamserver) TsConsoleSearch(agentId int64, query string, limit, offset int, username string) ([]byte, error) {
	teamMode := true
	if client, ok := ts.Broker.GetClient(username); ok {
		teamMode = client.ConsoleTeamMode()
	}

	hits, total, err := ts.DBMS.DbConsoleSearch(agentId, query, limit, offset, username, teamMode)
	if err != nil {
		return nil, err
	}

	items := make([]ConsoleSearchHitItem, 0, len(hits))
	for _, h := range hits {
		item := ConsoleSearchHitItem{
			Id:      h.Id,
			Snippet: h.Snippet,
		}
		if len(h.Packet) > 0 && json.Valid(h.Packet) {
			item.Packet = json.RawMessage(h.Packet)
		}
		items = append(items, item)
	}
	return json.Marshal(ConsoleSearchPage{
		Items:  items,
		Total:  total,
		Limit:  limit,
		Offset: offset,
	})
}

func (ts *Teamserver) TsConsoleGetAround(agentId int64, centerId int64, limit int, username string) ([]byte, error) {
	teamMode := true
	if client, ok := ts.Broker.GetClient(username); ok {
		teamMode = client.ConsoleTeamMode()
	}

	raw, oldestId, err := ts.DBMS.DbConsoleGetAround(agentId, centerId, limit, username, teamMode)
	if err != nil {
		return nil, err
	}

	_, total, _, err := ts.DBMS.DbConsoleGetPage(agentId, 0, 1, username, teamMode)
	if err != nil {
		total = len(raw)
	}

	items := make([]json.RawMessage, 0, len(raw))
	for _, message := range raw {
		items = append(items, json.RawMessage(message))
	}

	hasMore := false
	if oldestId > 0 {
		older, _, _, errOlder := ts.DBMS.DbConsoleGetPage(agentId, oldestId, 1, username, teamMode)
		hasMore = errOlder == nil && len(older) > 0
	}
	return json.Marshal(ConsolePage{
		Items:    items,
		Total:    total,
		OldestId: oldestId,
		HasMore:  hasMore,
	})
}
