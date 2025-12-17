package server

import (
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsTaskRunningExists(agentId string, taskId string) bool {
	return ts.TaskManager.RunningExists(agentId, taskId)
}

func (ts *Teamserver) TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData) {
	ts.TaskManager.Create(agentId, cmdline, client, taskData)
}

func (ts *Teamserver) TsTaskUpdate(agentId string, updateData adaptix.TaskData) {
	ts.TaskManager.Update(agentId, updateData)
}

func (ts *Teamserver) TsTaskPostHook(hookData adaptix.TaskData, jobIndex int) error {
	return ts.TaskManager.PostHook(hookData, jobIndex)
}

func (ts *Teamserver) TsTaskCancel(agentId string, taskId string) error {
	return ts.TaskManager.Cancel(agentId, taskId)
}

func (ts *Teamserver) TsTaskDelete(agentId string, taskId string) error {
	return ts.TaskManager.Delete(agentId, taskId)
}

func (ts *Teamserver) TsTaskSave(taskData adaptix.TaskData) error {
	return ts.TaskManager.Save(taskData)
}

///// Get Tasks

func (ts *Teamserver) getAgent(agentId string) (*Agent, error) {
	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil, fmt.Errorf("agent %v not found", agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return nil, fmt.Errorf("invalid agent type for '%v'", agentId)
	}
	return agent, nil
}

func (ts *Teamserver) extractHostedTasks(agent *Agent, availableSize int, startSize int, maxCount int) (tasks []adaptix.TaskData, sendTasks []string, usedSize int) {
	usedSize = startSize
	count := 0
	for {
		if maxCount > 0 && count >= maxCount {
			break
		}
		item, err := agent.HostedTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if usedSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			if taskData.Sync || taskData.Type == TYPE_BROWSER {
				agent.RunningTasks.Put(taskData.TaskId, taskData)
			}
			sendTasks = append(sendTasks, taskData.TaskId)
			usedSize += len(taskData.Data)
			count++
		} else {
			agent.HostedTasks.PushFront(taskData)
			break
		}
	}
	return
}

func (ts *Teamserver) extractTunnelTasks(agent *Agent, availableSize int, startSize int) (tasks []adaptix.TaskData, usedSize int) {
	usedSize = startSize
	for {
		item, err := agent.HostedTunnelTasks.Pop()
		if err != nil {
			break
		}
		taskData := item.(adaptix.TaskData)

		if usedSize+len(taskData.Data) < availableSize {
			tasks = append(tasks, taskData)
			usedSize += len(taskData.Data)
		} else {
			agent.HostedTunnelTasks.PushFront(taskData)
			break
		}
	}
	return
}

func (ts *Teamserver) extractTunnelData(agent *Agent, availableSize int, startSize int) (tasks []adaptix.TaskData, usedSize int) {
	usedSize = startSize
	for {
		item, err := agent.HostedTunnelData.Pop()
		if err != nil {
			break
		}
		taskDataTunnel := item.(adaptix.TaskDataTunnel)

		if usedSize+len(taskDataTunnel.Data.Data) < availableSize {
			tasks = append(tasks, taskDataTunnel.Data)
			usedSize += len(taskDataTunnel.Data.Data)
		} else {
			agent.HostedTunnelData.PushFront(taskDataTunnel)
			break
		}
	}
	return
}

func (ts *Teamserver) extractPivotTasks(agent *Agent, availableSize int, startSize int) (tasks []adaptix.TaskData, usedSize int) {
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
		data, err := ts.TsAgentGetHostedAll(pivotData.ChildAgentId, lostSize)
		if err != nil {
			continue
		}
		pivotTaskData, err := ts.Extender.ExAgentPivotPackData(agent.GetData().Name, pivotData.PivotId, data)
		if err != nil {
			continue
		}
		tasks = append(tasks, pivotTaskData)
		usedSize += len(pivotTaskData.Data)
	}
	return
}

func (ts *Teamserver) TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: %w", err)
	}

	var tasks []adaptix.TaskData

	hostedTasks, sendTasks, size := ts.extractHostedTasks(agent, availableSize, 0, -1)
	tasks = append(tasks, hostedTasks...)
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	tunnelTasks, size := ts.extractTunnelTasks(agent, availableSize, size)
	tasks = append(tasks, tunnelTasks...)

	tunnelData, size := ts.extractTunnelData(agent, availableSize, size)
	tasks = append(tasks, tunnelData...)

	pivotTasks, _ := ts.extractPivotTasks(agent, availableSize, size)
	tasks = append(tasks, pivotTasks...)

	return tasks, nil
}

func (ts *Teamserver) TsTaskGetAvailableTasks(agentId string, availableSize int) ([]adaptix.TaskData, int, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, 0, fmt.Errorf("TsTaskQueueGetAvailable: %w", err)
	}

	var tasks []adaptix.TaskData

	hostedTasks, sendTasks, size := ts.extractHostedTasks(agent, availableSize, 0, -1)
	tasks = append(tasks, hostedTasks...)
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	tunnelTasks, size := ts.extractTunnelTasks(agent, availableSize, size)
	tasks = append(tasks, tunnelTasks...)

	return tasks, size, nil
}

func (ts *Teamserver) TsTaskGetAvailableTasksCount(agentId string, maxCount int, availableSize int) ([]adaptix.TaskData, int, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, 0, fmt.Errorf("TsTaskQueueGetAvailable: %w", err)
	}

	// Extract hosted tasks with count limit
	tasks, sendTasks, size := ts.extractHostedTasks(agent, availableSize, 0, maxCount)
	if len(sendTasks) > 0 {
		packet := CreateSpAgentTaskSend(sendTasks)
		ts.TsSyncAllClients(packet)
	}

	return tasks, size, nil
}

/// Get Pivot Tasks

func (ts *Teamserver) TsTasksPivotExists(agentId string, first bool) bool {
	return ts.tsTasksPivotExistsWithVisited(agentId, first, make(map[string]bool))
}

func (ts *Teamserver) tsTasksPivotExistsWithVisited(agentId string, first bool, visited map[string]bool) bool {
	if visited[agentId] {
		return false
	}
	visited[agentId] = true

	agent, err := ts.getAgent(agentId)
	if err != nil {
		return false
	}

	if !first {
		if agent.HostedTasks.Len() > 0 || agent.HostedTunnelTasks.Len() > 0 || agent.HostedTunnelData.Len() > 0 {
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

func (ts *Teamserver) TsTaskGetAvailablePivotAll(agentId string, availableSize int) ([]adaptix.TaskData, error) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return nil, fmt.Errorf("TsTaskQueueGetAvailable: %w", err)
	}

	tasks, _ := ts.extractPivotTasks(agent, availableSize, 0)
	return tasks, nil
}

func (ts *Teamserver) TsProcessHookJobsForDisconnectedClient(clientName string) {
	ts.TaskManager.ProcessDisconnectedClient(clientName)
}
