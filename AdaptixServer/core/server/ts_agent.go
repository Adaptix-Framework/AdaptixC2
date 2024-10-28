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

	packet := CreateSpAgentNew(agentInfo.AgentName, agentInfo.ListenerName, agentInfo.AgentUI)
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

	data, err = ts.Extender.AgentCreate(agentName, beat)
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

	return nil
}
