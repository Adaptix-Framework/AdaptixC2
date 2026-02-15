package server

import (
	"AdaptixServer/core/extender"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"

	adaptix "github.com/Adaptix-Framework/axc2"
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

	if ts.listener_configs.Contains(listenerInfo.Name) {
		return fmt.Errorf("listener %v already exists", listenerInfo.Name)
	}

	ts.listener_configs.Put(listenerInfo.Name, listenerInfo)

	return nil
}

func (ts *Teamserver) TsListenerRegByName(listenerName string) (string, error) {

	value, ok := ts.listeners.Get(listenerName)
	if !ok {
		return "", errors.New("listener not found: " + listenerName)
	}

	return value.(adaptix.ListenerData).RegName, nil
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

	if ts.ScriptManager != nil && agentInfo.AX != "" {
		err := ts.TsAxScriptLoadAgent(agentInfo.Name, agentInfo.AX, agentInfo.Listeners)
		if err != nil {
			logs.Warn("", "Agent %s: AxScript load failed (commands will come from client): %v", agentInfo.Name, err)
		} else {
			ts.TsAxScriptBroadcastCommands()
		}
	}

	return nil
}

func (ts *Teamserver) TsServiceReg(serviceInfo extender.ServiceInfo) error {

	if !isvalid.ValidSBNString(serviceInfo.Name) {
		return errors.New("invalid service name (must only contain letters and numbers): " + serviceInfo.Name)
	}

	if ts.service_configs.Contains(serviceInfo.Name) {
		return fmt.Errorf("service %v already exists", serviceInfo.Name)
	}

	ts.service_configs.Put(serviceInfo.Name, serviceInfo)

	return nil
}

func (ts *Teamserver) TsServiceUnreg(serviceName string) error {
	if !ts.service_configs.Contains(serviceName) {
		return fmt.Errorf("service %v not found", serviceName)
	}

	ts.service_configs.Delete(serviceName)

	return nil
}
