package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"fmt"
)

func (ts *Teamserver) TsListenerReg(listenerInfo extender.ListenerInfo) error {

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.Type, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)

	if ts.listener_configs.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.listener_configs.Put(listenerFN, listenerInfo)

	return nil
}

func (ts *Teamserver) TsAgentReg(agentInfo extender.AgentInfo) error {
	if ts.agent_configs.Contains(agentInfo.AgentName) {
		return fmt.Errorf("agent %v already exists", agentInfo.AgentName)
	}
	agentCrc := krypt.CRC32([]byte(agentInfo.AgentName))
	agentMark := fmt.Sprintf("%08x", agentCrc)

	ts.agent_types.Put(agentMark, agentInfo.AgentName)
	ts.agent_configs.Put(agentInfo.AgentName, agentInfo)

	return nil
}
