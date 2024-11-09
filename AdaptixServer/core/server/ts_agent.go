package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"fmt"
	"time"
)

func (ts *Teamserver) AgentNew(agentInfo extender.AgentInfo) error {
	if ts.agent_configs.Contains(agentInfo.AgentName) {
		return fmt.Errorf("agent %v already exists", agentInfo.AgentName)
	}
	agentCrc := krypt.CRC32([]byte(agentInfo.AgentName))
	agentMark := fmt.Sprintf("%08x", agentCrc)

	ts.agent_types.Put(agentMark, agentInfo.AgentName)
	ts.agent_configs.Put(agentInfo.AgentName, agentInfo)

	packet := CreateSpAgentReg(agentInfo.AgentName, agentInfo.ListenerName, agentInfo.AgentUI, agentInfo.AgentCmd)
	ts.SyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) AgentRequest(agentCrc string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error) {
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

		data, err = ts.Extender.AgentCreate(agentName, beat)
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
		ts.SyncAllClients(packetNew)
		ts.SyncSavePacket(packetNew.store, packetNew)

	} else {
		agent, _ = value.(*Agent)
	}

	packetTick := CreateSpAgentTick(agent.Data.Id)
	ts.SyncAllClients(packetTick)

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

		_ = json.NewEncoder(&agentBuffer).Encode(agent.Data)

		respData, err = ts.Extender.AgentPackData(agentName, agentBuffer.Bytes(), agentTasksData)
		if err != nil {
			return nil, err
		}
	}

	return respData, nil
}

func (ts *Teamserver) AgentCommand(agentName string, agentId string, username string, cmdline string, args map[string]any) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    TaskData
		data        []byte
	)

	if ts.agent_configs.Contains(agentName) {

		value, ok := ts.agents.Get(agentId)
		if ok {

			agent, _ = value.(*Agent)
			_ = json.NewEncoder(&agentObject).Encode(agent.Data)

			data, err = ts.Extender.AgentCommand(agentName, agentObject.Bytes(), args)
			if err != nil {
				return err
			}

			err = json.Unmarshal(data, &taskData)
			if err != nil {
				return err
			}

			taskData.CommandLine = cmdline
			taskData.AgentId = agentId
			if taskData.TaskId == "" {
				taskData.TaskId, _ = krypt.GenerateUID(8)
			}

			agent.TasksQueue.Put(taskData)

			packet := CreateSpAgentTask(taskData, username)
			ts.SyncAllClients(packet)
			ts.SyncSavePacket(packet.store, packet)

		} else {
			return fmt.Errorf("agent '%v' does not exist", agentId)
		}
	} else {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	return nil
}
