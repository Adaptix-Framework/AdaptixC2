package server

import (
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"strings"
)

/// AGENT

func (ts *Teamserver) TsAgentGuiDisks(agentId string, clientName string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentBrowserDisks(agent.Data)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agentId, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsAgentGuiProcess(agentId string, clientName string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentBrowserProcess(agent.Data)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agentId, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsAgentGuiFiles(agentId string, path string, clientName string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentBrowserFiles(agent.Data, path)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agentId, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsAgentGuiUpload(agentId string, path string, content []byte, clientName string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentBrowserUpload(agent.Data, path, content)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agentId, "", clientName, taskData)
	return nil
}

/// SYNC

func (ts *Teamserver) TsClientGuiDisks(taskData adaptix.TaskData, jsonDrives string) {
	value, ok := ts.agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserDisks(taskData, jsonDrives)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientGuiFiles(taskData adaptix.TaskData, path string, jsonFiles string) {
	value, ok := ts.agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	for len(path) > 0 && (strings.HasSuffix(path, "\\") || (path != "/" && strings.HasSuffix(path, "/"))) {
		path = path[:len(path)-1]
	}

	packet := CreateSpBrowserFiles(taskData, path, jsonFiles)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientGuiFilesStatus(taskData adaptix.TaskData) {
	value, ok := ts.agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	packet := CreateSpBrowserFilesStatus(taskData)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientGuiProcess(taskData adaptix.TaskData, jsonFiles string) {
	value, ok := ts.agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserProcess(taskData, jsonFiles)
	ts.TsSyncClient(task.Client, packet)
}
