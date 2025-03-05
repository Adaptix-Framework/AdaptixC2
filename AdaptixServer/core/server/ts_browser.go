package server

import (
	"AdaptixServer/core/adaptix"
	"bytes"
	"encoding/json"
	"fmt"
	"strings"
)

/// AGENT

func (ts *Teamserver) TsAgentBrowserDisks(agentId string, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentBrowserDisks(agent.Data.Name, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserProcess(agentId string, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentBrowserProcess(agent.Data.Name, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserFiles(agentId string, path string, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentBrowserFiles(agent.Data.Name, path, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserUpload(agentId string, path string, content []byte, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentBrowserUpload(agent.Data.Name, path, content, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserDownload(agentId string, path string, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentBrowserDownload(agent.Data.Name, path, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentCtxExit(agentId string, clientName string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		data        []byte
	)

	value, ok := ts.agents.Get(agentId)
	if ok {

		agent, _ = value.(*Agent)
		_ = json.NewEncoder(&agentObject).Encode(agent.Data)

		data, err = ts.Extender.ExAgentCtxExit(agent.Data.Name, agentObject.Bytes())
		if err != nil {
			return err
		}

		ts.TsTaskCreate(agentId, "agent terminate", clientName, data)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

/// SYNC

func (ts *Teamserver) TsClientBrowserDisks(jsonTask string, jsonDrives string) {
	var (
		agent    *Agent
		task     adaptix.TaskData
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)

	err = json.Unmarshal([]byte(jsonTask), &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(taskData.AgentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return
	}

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

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

func (ts *Teamserver) TsClientBrowserFiles(jsonTask string, path string, jsonFiles string) {
	var (
		agent    *Agent
		task     adaptix.TaskData
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)

	err = json.Unmarshal([]byte(jsonTask), &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(taskData.AgentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return
	}

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	for len(path) > 0 && (strings.HasSuffix(path, "\\") || strings.HasSuffix(path, "/")) {
		path = path[:len(path)-1]
	}

	packet := CreateSpBrowserFiles(taskData, path, jsonFiles)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientBrowserFilesStatus(jsonTask string) {
	var (
		agent    *Agent
		task     adaptix.TaskData
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)

	err = json.Unmarshal([]byte(jsonTask), &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(taskData.AgentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return
	}

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.RunningTasks.Delete(taskData.TaskId)

	packet := CreateSpBrowserFilesStatus(taskData)
	ts.TsSyncClient(task.Client, packet)
}

func (ts *Teamserver) TsClientBrowserProcess(jsonTask string, jsonFiles string) {
	var (
		agent    *Agent
		task     adaptix.TaskData
		taskData adaptix.TaskData
		value    any
		ok       bool
		err      error
	)

	err = json.Unmarshal([]byte(jsonTask), &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(taskData.AgentId)
	if ok {
		agent = value.(*Agent)
	} else {
		return
	}

	value, ok = agent.RunningTasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

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
