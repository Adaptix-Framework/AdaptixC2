package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"encoding/json"
	"fmt"
	"time"
)

func (ts *Teamserver) AgentNew(agentInfo extender.AgentInfo) error {
	if ts.agent_configs.Contains(agentInfo.AgentName) {
		return fmt.Errorf("agent %v already exists", agentInfo.AgentName)
	}
	agentCrc := krypt.CRC32([]byte(agentInfo.AgentName))
	agentType := fmt.Sprintf("%08x", agentCrc)

	ts.agent_types.Put(agentType, agentInfo.AgentName)
	ts.agent_configs.Put(agentInfo.AgentName, agentInfo)

	packet := CreateSpAgentReg(agentInfo.AgentName, agentInfo.ListenerName, agentInfo.AgentUI, agentInfo.AgentCmd)
	ts.SyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) AgentRequest(agentType string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) error {
	var (
		agentName string
		data      []byte
		err       error
		agentData AgentData
	)

	value, ok := ts.agent_types.Get(agentType)
	if !ok {
		return fmt.Errorf("agent type %v does not exists", agentType)
	}
	agentName = value.(string)

	data, err = ts.Extender.AgentCreateData(agentName, beat)
	if err != nil {
		return err
	}

	err = json.Unmarshal(data, &agentData)
	if err != nil {
		return err
	}

	agentData.ExternalIP = ExternalIP
	agentData.Type = agentType
	agentData.Name = agentName
	agentData.Listener = listenerName
	agentData.CreateTime = time.Now().Unix()
	agentData.LastTick = int(time.Now().Unix())
	if len(agentData.Tags) == 0 {
		agentData.Tags = []string{}
	}

	if !ts.agents.Contains(agentData.Id) {
		ts.agents.Put(agentData.Id, agentData)

		packetNew := CreateSpAgentNew(agentData)
		ts.SyncAllClients(packetNew)
		ts.SyncSavePacket(packetNew.store, packetNew)
	}

	if len(bodyData) > 4 {
		ts.Extender.AgentProcess(agentData.Type, agentData.Id, bodyData)
	}

	packetTick := CreateSpAgentTick(agentData.Id)
	ts.SyncAllClients(packetTick)

	// AgentResponse

	return nil
}
