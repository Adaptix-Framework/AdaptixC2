package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"bytes"
	"encoding/json"
	"fmt"
	"strings"
	"time"
)

/// AGENT

func (ts *Teamserver) TsAgentBrowserDisks(agentId string, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.StartDate = time.Now().Unix()

		agent.TasksQueue.Put(taskData)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserProcess(agentId string, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.StartDate = time.Now().Unix()

		agent.TasksQueue.Put(taskData)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserFiles(agentId string, path string, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.StartDate = time.Now().Unix()

		agent.TasksQueue.Put(taskData)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserUpload(agentId string, path string, content []byte, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.StartDate = time.Now().Unix()

		agent.TasksQueue.Put(taskData)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentBrowserDownload(agentId string, path string, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.StartDate = time.Now().Unix()

		agent.TasksQueue.Put(taskData)

	} else {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	return nil
}

func (ts *Teamserver) TsAgentCtxExit(agentId string, username string) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
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

		err = json.Unmarshal(data, &taskData)
		if err != nil {
			return err
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agentId
		taskData.User = username
		taskData.CommandLine = "agent terminate"
		taskData.StartDate = time.Now().Unix()
		taskData.Sync = true

		agent.TasksQueue.Put(taskData)

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

	value, ok = agent.Tasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.Tasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserDisks(taskData, jsonDrives)
	ts.TsSyncClient(task.User, packet)
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

	value, ok = agent.Tasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.Tasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	for len(path) > 0 && (strings.HasSuffix(path, "\\") || strings.HasSuffix(path, "/")) {
		path = path[:len(path)-1]
	}

	packet := CreateSpBrowserFiles(taskData, path, jsonFiles)
	ts.TsSyncClient(task.User, packet)
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

	value, ok = agent.Tasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.Tasks.Delete(taskData.TaskId)

	packet := CreateSpBrowserFilesStatus(taskData)
	ts.TsSyncClient(task.User, packet)
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

	value, ok = agent.Tasks.Get(taskData.TaskId)
	if ok {
		task = value.(adaptix.TaskData)
	} else {
		return
	}

	if task.Type != TYPE_BROWSER {
		return
	}

	agent.Tasks.Delete(taskData.TaskId)

	if taskData.MessageType != CONSOLE_OUT_ERROR && taskData.MessageType != CONSOLE_OUT_LOCAL_ERROR {
		taskData.Message = "Status: OK"
	}

	packet := CreateSpBrowserProcess(taskData, jsonFiles)
	ts.TsSyncClient(task.User, packet)
}
