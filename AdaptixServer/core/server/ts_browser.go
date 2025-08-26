package server

import (
	"strings"

	"github.com/Adaptix-Framework/axc2"
)

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
