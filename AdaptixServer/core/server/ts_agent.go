package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"fmt"
	"time"
)

func (ts *Teamserver) TsAgentNew(agentInfo extender.AgentInfo) error {
	if ts.agent_configs.Contains(agentInfo.AgentName) {
		return fmt.Errorf("agent %v already exists", agentInfo.AgentName)
	}
	agentCrc := krypt.CRC32([]byte(agentInfo.AgentName))
	agentMark := fmt.Sprintf("%08x", agentCrc)

	ts.agent_types.Put(agentMark, agentInfo.AgentName)
	ts.agent_configs.Put(agentInfo.AgentName, agentInfo)

	packet := CreateSpAgentReg(agentInfo.AgentName, agentInfo.ListenerName, agentInfo.AgentUI, agentInfo.AgentCmd)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) TsAgentRequest(agentCrc string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error) {
	var (
		agentName      string
		data           []byte
		respData       []byte
		err            error
		agent          *Agent
		agentData      AgentData
		agentTasksData [][]byte
		agentBuffer    bytes.Buffer
	)

	value, ok := ts.agent_types.Get(agentCrc)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentCrc)
	}
	agentName = value.(string)

	value, ok = ts.agents.Get(agentId)
	if !ok {

		data, err = ts.Extender.ExAgentCreate(agentName, beat)
		if err != nil {
			return nil, err
		}
		err = json.Unmarshal(data, &agentData)
		if err != nil {
			return nil, err
		}

		agentData.Crc = agentCrc
		agentData.Name = agentName
		agentData.Id = agentId
		agentData.Listener = listenerName
		agentData.ExternalIP = ExternalIP
		agentData.CreateTime = time.Now().Unix()
		agentData.LastTick = int(time.Now().Unix())
		if len(agentData.Tags) == 0 {
			agentData.Tags = []string{}
		}

		agent = &Agent{
			Data:        agentData,
			TasksQueue:  safe.NewSlice(),
			Tasks:       safe.NewMap(),
			ClosedTasks: safe.NewMap(),
		}

		ts.agents.Put(agentData.Id, agent)

		packetNew := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packetNew)
		ts.TsSyncSavePacket(packetNew.store, packetNew)

	} else {
		agent, _ = value.(*Agent)
	}

	packetTick := CreateSpAgentTick(agent.Data.Id)
	ts.TsSyncAllClients(packetTick)

	_ = json.NewEncoder(&agentBuffer).Encode(agent.Data)

	if len(bodyData) > 4 {
		_, _ = ts.Extender.ExAgentProcessData(agentName, agentBuffer.Bytes(), bodyData)
	}

	if agent.TasksQueue.Len() > 0 {
		tasksArray := agent.TasksQueue.CutArray()
		for _, value := range tasksArray {
			task, ok := value.(TaskData)
			if ok {
				var taskBuffer bytes.Buffer
				_ = json.NewEncoder(&taskBuffer).Encode(task)
				agentTasksData = append(agentTasksData, taskBuffer.Bytes())
				agent.Tasks.Put(task.TaskId, task)
			}
		}

		respData, err = ts.Extender.ExAgentPackData(agentName, agentBuffer.Bytes(), agentTasksData)
		if err != nil {
			return nil, err
		}

		message := fmt.Sprintf("Agent called server, sent [%v] bytes", len(respData))
		ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "")
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, username string, cmdline string, args map[string]any) error {
	var (
		err         error
		agentObject bytes.Buffer
		messageInfo string
		agent       *Agent
		taskData    TaskData
		data        []byte
	)

	if ts.agent_configs.Contains(agentName) {

		value, ok := ts.agents.Get(agentId)
		if ok {

			agent, _ = value.(*Agent)
			_ = json.NewEncoder(&agentObject).Encode(agent.Data)

			data, messageInfo, err = ts.Extender.ExAgentCommand(agentName, agentObject.Bytes(), args)
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
			taskData.CommandLine = cmdline
			taskData.AgentId = agentId
			taskData.User = username
			taskData.StartDate = time.Now().Unix()

			agent.TasksQueue.Put(taskData)

			packet := CreateSpAgentTaskCreate(taskData)
			ts.TsSyncAllClients(packet)
			ts.TsSyncSavePacket(packet.store, packet)

			if len(messageInfo) > 0 {
				ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, messageInfo, "")
			}

		} else {
			return fmt.Errorf("agent '%v' does not exist", agentId)
		}
	} else {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	return nil
}

func (ts *Teamserver) TsAgentTaskUpdate(agentId string, taskObject []byte) {
	var (
		agent    *Agent
		task     TaskData
		taskData TaskData
		value    any
		ok       bool
		err      error
	)
	err = json.Unmarshal(taskObject, &taskData)
	if err != nil {
		return
	}

	value, ok = ts.agents.Get(agentId)
	if ok {
		agent = value.(*Agent)
	} else {
		logs.Error("TsAgentTaskUpdate: agent %v not found", agentId)
		return
	}

	value, ok = agent.Tasks.GetDelete(taskData.TaskId)
	if ok {
		task = value.(TaskData)
		task.Data = []byte("")
		task.FinishDate = taskData.FinishDate
		task.MessageType = taskData.MessageType
		task.Message = taskData.Message
		task.ClearText = taskData.ClearText
		task.Completed = taskData.Completed
	} else {
		task = taskData
		logs.Error("TsAgentTaskUpdate: task %v not found", taskData.TaskId)
	}

	if task.Completed {
		agent.ClosedTasks.Put(task.TaskId, task)
	} else {
		agent.Tasks.Put(task.TaskId, task)
	}

	if task.Sync {
		packet := CreateSpAgentTaskUpdate(task)
		ts.TsSyncAllClients(packet)
		ts.TsSyncSavePacket(packet.store, packet)
	}

	return
}

func (ts *Teamserver) TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)
}
