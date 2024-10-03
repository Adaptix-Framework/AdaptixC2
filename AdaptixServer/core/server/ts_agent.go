package server

import (
	"AdaptixServer/core/extender"
	"fmt"
)

func (ts *Teamserver) AgentNew(agentInfo extender.AgentInfo) error {

	if ts.agent_configs.Contains(agentInfo.AgentName) {
		return fmt.Errorf("agent %v already exists", agentInfo.AgentName)
	}

	ts.listener_configs.Put(agentInfo.AgentName, agentInfo)

	packet := CreateSpAgentNew(agentInfo.AgentName, agentInfo.ListenerName, agentInfo.AgentUI)
	ts.SyncSavePacket(packet.store, packet)

	return nil
}
