package server

import (
	"encoding/json"
	"strings"

	"github.com/Adaptix-Framework/axc2"
)

/// SYNC

func (ts *Teamserver) TsClientGuiDisksWindows(taskData adaptix.TaskData, drives []adaptix.ListingDrivesDataWin) {

	jsonDrives, err := json.Marshal(drives)
	if err != nil {
		return
	}

	value, ok := ts.Agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != adaptix.TASK_TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserDisks(taskData, string(jsonDrives))
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientGuiFilesWindows(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataWin) {
	jsonFiles, err := json.Marshal(files)
	if err != nil {
		return
	}

	ts.TsClientGuiFiles(taskData, path, string(jsonFiles))
}

func (ts *Teamserver) TsClientGuiFilesUnix(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataUnix) {
	jsonFiles, err := json.Marshal(files)
	if err != nil {
		return
	}
	ts.TsClientGuiFiles(taskData, path, string(jsonFiles))
}

func (ts *Teamserver) TsClientGuiFiles(taskData adaptix.TaskData, path string, jsonFiles string) {
	value, ok := ts.Agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != adaptix.TASK_TYPE_BROWSER {
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
	value, ok := ts.Agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != adaptix.TASK_TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	packet := CreateSpBrowserFilesStatus(taskData)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientGuiProcessWindows(taskData adaptix.TaskData, process []adaptix.ListingProcessDataWin) {
	jsonProcess, err := json.Marshal(process)
	if err != nil {
		return
	}

	ts.TsClientGuiProcess(taskData, string(jsonProcess))
}

func (ts *Teamserver) TsClientGuiProcessUnix(taskData adaptix.TaskData, process []adaptix.ListingProcessDataUnix) {
	jsonProcess, err := json.Marshal(process)
	if err != nil {
		return
	}

	ts.TsClientGuiProcess(taskData, string(jsonProcess))
}

func (ts *Teamserver) TsClientGuiProcess(taskData adaptix.TaskData, jsonFiles string) {
	value, ok := ts.Agents.Get(taskData.AgentId)
	if !ok {
		return
	}
	agent := value.(*Agent)

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if !ok {
		return
	}
	task := value.(adaptix.TaskData)

	if task.Type != adaptix.TASK_TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserProcess(taskData, jsonFiles)
	ts.TsSyncClient(task.Client, packet)
}
