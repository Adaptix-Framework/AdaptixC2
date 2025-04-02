package server

import (
	"AdaptixServer/core/extender"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"
)

func (ts *Teamserver) TsListenerReg(listenerInfo extender.ListenerInfo) error {

	if listenerInfo.Type != "internal" && listenerInfo.Type != "external" {
		return errors.New("invalid listener type: must be internal or external")
	}

	if !isvalid.ValidSBNString(listenerInfo.Protocol) {
		return errors.New("invalid listener protocol (must only contain letters and numbers): " + listenerInfo.Protocol)
	}

	if !isvalid.ValidSBNString(listenerInfo.Name) {
		return errors.New("invalid listener name (must only contain letters and numbers): " + listenerInfo.Type)
	}

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.Type, listenerInfo.Protocol, listenerInfo.Name)

	if ts.listener_configs.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.listener_configs.Put(listenerFN, listenerInfo)

	return nil
}

func (ts *Teamserver) TsAgentReg(agentInfo extender.AgentInfo) error {

	if ts.agent_configs.Contains(agentInfo.Name) {
		return fmt.Errorf("agent %v already exists", agentInfo.Name)
	}

	if !isvalid.ValidHex8(agentInfo.Watermark) {
		return fmt.Errorf("agent %s has invalid watermark %s... must be 8 digit hex value", agentInfo.Name, agentInfo.Watermark)
	}

	ts.wm_agent_types[agentInfo.Watermark] = agentInfo.Name
	ts.agent_configs.Put(agentInfo.Name, agentInfo)

	return nil
}
