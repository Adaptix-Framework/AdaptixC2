package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"AdaptixServer/core/utils/tformat"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"time"
)

func (ts *Teamserver) TsAgentUpdateData(newAgentObject []byte) error {
	var (
		agent        *Agent
		err          error
		newAgentData adaptix.AgentData
	)

	err = json.Unmarshal(newAgentObject, &newAgentData)
	if err != nil {
		return err
	}

	value, ok := ts.agents.Get(newAgentData.Id)
	if !ok {
		return errors.New("Agent does not exist")
	}

	agent, _ = value.(*Agent)
	agent.Data.Sleep = newAgentData.Sleep
	agent.Data.Jitter = newAgentData.Jitter
	agent.Data.Elevated = newAgentData.Elevated
	agent.Data.Username = newAgentData.Username

	err = ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentGenetate(agentName string, config string, listenerProfile []byte) ([]byte, string, error) {
	return ts.Extender.ExAgentGenerate(agentName, config, listenerProfile)
}

func (ts *Teamserver) TsAgentRequest(agentCrc string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error) {
	var (
		agentName      string
		data           []byte
		respData       []byte
		err            error
		agent          *Agent
		agentData      adaptix.AgentData
		agentTasksData [][]byte
		agentBuffer    bytes.Buffer
	)

	value, ok := ts.agent_types.Get(agentCrc)
	if !ok {
		return nil, fmt.Errorf("agent type %v does not exists", agentCrc)
	}
	agentName = value.(string)

	/// CREATE OR GET AGENT

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
		agentData.Tags = ""

		agent = &Agent{
			Data:        agentData,
			TunnelQueue: safe.NewSlice(),
			TasksQueue:  safe.NewSlice(),
			Tasks:       safe.NewMap(),
			ClosedTasks: safe.NewMap(),
			Tick:        false,
		}

		ts.agents.Put(agentData.Id, agent)

		err = ts.DBMS.DbAgentInsert(agentData)
		if err != nil {
			logs.Error("", err.Error())
		}

		packetNew := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packetNew)

		message := fmt.Sprintf("New '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
		packet2 := CreateSpEvent(EVENT_AGENT_NEW, message)
		ts.TsSyncAllClients(packet2)
		ts.events.Put(packet2)

	} else {
		agent, _ = value.(*Agent)
	}

	/// AGENT TICK

	if agent.Data.Async {
		agent.Data.LastTick = int(time.Now().Unix())
		ts.DBMS.DbAgentUpdate(agent.Data)
		agent.Tick = true
	}

	/// PROCESS RECEIVED DATA FROM AGENT

	_ = json.NewEncoder(&agentBuffer).Encode(agent.Data)
	if len(bodyData) > 4 {
		_, _ = ts.Extender.ExAgentProcessData(agentName, agentBuffer.Bytes(), bodyData)
	}

	/// SEND NEW DATA TO AGENT

	tasksCount := agent.TasksQueue.Len()
	tunnelTasksCount := agent.TunnelQueue.Len()
	if tasksCount > 0 || tunnelTasksCount > 0 {
		respData, err = ts.Extender.ExAgentPackData(agentName, agentBuffer.Bytes(), agentTasksData)
		if err != nil {
			return nil, err
		}

		if tasksCount > 0 {
			message := fmt.Sprintf("Agent called server, sent [%v]", tformat.SizeBytesToFormat(uint64(len(respData))))
			ts.TsAgentConsoleOutput(agentId, CONSOLE_OUT_INFO, message, "")
		}
	}

	return respData, nil
}

func (ts *Teamserver) TsAgentCommand(agentName string, agentId string, clientName string, cmdline string, args map[string]any) error {
	var (
		err         error
		agentObject bytes.Buffer
		agent       *Agent
		taskData    adaptix.TaskData
		messageData adaptix.ConsoleMessageData
		dataTask    []byte
		dataMessage []byte
	)

	if ts.agent_configs.Contains(agentName) {

		value, ok := ts.agents.Get(agentId)
		if ok {

			agent, _ = value.(*Agent)
			_ = json.NewEncoder(&agentObject).Encode(agent.Data)

			dataTask, dataMessage, err = ts.Extender.ExAgentCommand(agentName, agentObject.Bytes(), args)
			if err != nil {
				return err
			}

			err = json.Unmarshal(dataTask, &taskData)
			if err != nil {
				return err
			}

			err = json.Unmarshal(dataMessage, &messageData)
			if err != nil {
				return err
			}

			if taskData.TaskId == "" {
				taskData.TaskId, _ = krypt.GenerateUID(8)
			}
			taskData.CommandLine = cmdline
			taskData.AgentId = agentId
			taskData.Client = clientName
			taskData.Computer = agent.Data.Computer
			taskData.StartDate = time.Now().Unix()

			if taskData.Sync {
				packet := CreateSpAgentTaskCreate(taskData)
				ts.TsSyncAllClients(packet)
			}
			
			if taskData.Type == TYPE_TASK || taskData.Type == TYPE_BROWSER || taskData.Type == TYPE_JOB {
				agent.TasksQueue.Put(taskData)
			} else if taskData.Type == TYPE_TUNNEL {
				agent.Tasks.Put(taskData.TaskId, taskData)
			}

			if len(messageData.Message) > 0 || len(messageData.Text) > 0 {
				ts.TsAgentConsoleOutput(agentId, messageData.Status, messageData.Message, messageData.Text)
			}

		} else {
			return fmt.Errorf("agent '%v' does not exist", agentId)
		}
	} else {
		return fmt.Errorf("agent %v not registered", agentName)
	}

	return nil
}

func (ts *Teamserver) TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string) {
	packet := CreateSpAgentConsoleOutput(agentId, messageType, message, clearText)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsAgentRemove(agentId string) error {
	_, ok := ts.agents.GetDelete(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	err := ts.DBMS.DbAgentDelete(agentId)
	if err != nil {
		logs.Error("", err.Error())
	} else {
		ts.DBMS.DbTaskDelete("", agentId)
	}

	packet := CreateSpAgentRemove(agentId)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsAgentSetTag(agentId string, tag string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("Agent does not exist")
	}

	agent, _ := value.(*Agent)
	agent.Data.Tags = tag

	err := ts.DBMS.DbAgentUpdate(agent.Data)
	if err != nil {
		logs.Error("", err.Error())
	}

	packetNew := CreateSpAgentUpdate(agent.Data)
	ts.TsSyncAllClients(packetNew)

	return nil
}

func (ts *Teamserver) TsAgentTickUpdate() {
	for {
		var agentSlize []string
		ts.agents.ForEach(func(key string, value interface{}) bool {
			agent := value.(*Agent)
			if agent.Data.Async {
				if agent.Tick {
					agent.Tick = false
					agentSlize = append(agentSlize, agent.Data.Id)
				}
			}
			return true
		})

		if len(agentSlize) > 0 {
			packetTick := CreateSpAgentTick(agentSlize)
			ts.TsSyncAllClients(packetTick)
		}

		time.Sleep(800 * time.Millisecond)
	}
}
