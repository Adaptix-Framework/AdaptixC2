package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"
)

func (ts *Teamserver) TsListenerReg(listenerInfo extender.ListenerInfo) error {

	if listenerInfo.Type != "internal" && listenerInfo.Type != "external" {
		return errors.New("invalid listener type: must be internal or external")
	}

	if !isvalid.ValidSBNString(listenerInfo.ListenerProtocol) {
		return errors.New("invalid listener protocol (must only contain letters and numbers): " + listenerInfo.ListenerProtocol)
	}

	if !isvalid.ValidSBNString(listenerInfo.ListenerName) {
		return errors.New("invalid listener name (must only contain letters and numbers): " + listenerInfo.Type)
	}

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

	ts.wm_agent_types[agentMark] = agentInfo.AgentName
	ts.agent_configs.Put(agentInfo.AgentName, agentInfo)

	return nil
}
