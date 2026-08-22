package server

import (
	"AdaptixServer/core/extender"
	isvalid "AdaptixServer/core/utils/valid"
	"encoding/json"
	"errors"
	"fmt"
	"sort"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsListenerReg(listenerInfo extender.ListenerInfo) error {

	if listenerInfo.Type != "internal" && listenerInfo.Type != "external" && listenerInfo.Type != "bind" && listenerInfo.Type != "cloud" {
		return errors.New("invalid listener type: must be internal, external, bind or cloud")
	}

	if !isvalid.ValidSBNString(listenerInfo.Protocol) {
		return errors.New("invalid listener protocol (must only contain letters and numbers): " + listenerInfo.Protocol)
	}

	if !isvalid.ValidSBNString(listenerInfo.Name) {
		return errors.New("invalid listener name (must only contain letters and numbers): " + listenerInfo.Name)
	}

	if ts.listener_configs.Contains(listenerInfo.Name) {
		return fmt.Errorf("listener %v already exists", listenerInfo.Name)
	}

	ts.listener_configs.Put(listenerInfo.Name, listenerInfo)

	return nil
}

func (ts *Teamserver) TsListenerRegByName(listenerName string) (string, error) {
	listenerData, ok := ts.listeners.Get(listenerName)
	if !ok {
		return "", errors.New("listener not found: " + listenerName)
	}
	return listenerData.RegName, nil
}

func (ts *Teamserver) TsAgentReg(agentInfo extender.AgentInfo) error {

	if ts.agent_configs.Contains(agentInfo.Name) {
		return fmt.Errorf("agent %v already exists", agentInfo.Name)
	}

	if !isvalid.ValidHex8(agentInfo.Watermark) {
		return fmt.Errorf("agent %s has invalid watermark %s... must be 8 digit hex value", agentInfo.Name, agentInfo.Watermark)
	}

	ts.wm_agent_types.Put(agentInfo.Watermark, agentInfo.Name)
	ts.agent_configs.Put(agentInfo.Name, agentInfo)

	if ts.ScriptManager != nil && agentInfo.AX != "" {
		err := ts.TsAxScriptLoadAgent(agentInfo.Name, agentInfo.AX, agentInfo.Listeners)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusWarn, 0, "server", "extender", "Agent %s: AxScript load failed (commands will come from client): %v", agentInfo.Name, err)
		}
	}
	return nil
}

func (ts *Teamserver) TsAgentCatalog() (string, error) {
	out := make([]adaptix.AgentCatalogItem, 0)
	ts.agent_configs.ForEachFast(func(key string, info extender.AgentInfo) bool {
		out = append(out, adaptix.AgentCatalogItem{
			Name:      info.Name,
			Listeners: append([]string(nil), info.Listeners...),
			AXS:       info.AX,
		})
		return true
	})
	sort.Slice(out, func(i, j int) bool { return out[i].Name < out[j].Name })
	raw, err := json.Marshal(out)
	if err != nil {
		return "", err
	}
	return string(raw), nil
}

func (ts *Teamserver) TsServiceReg(serviceInfo extender.ServiceInfo) error {

	if !isvalid.ValidSBNString(serviceInfo.Name) {
		return errors.New("invalid service name (must only contain letters and numbers): " + serviceInfo.Name)
	}

	if ts.service_configs.Contains(serviceInfo.Name) {
		return fmt.Errorf("service %v already exists", serviceInfo.Name)
	}

	ts.service_configs.Put(serviceInfo.Name, serviceInfo)

	if serviceInfo.AX != "" && ts.ScriptManager != nil {
		if err := ts.ScriptManager.LoadServiceScript(serviceInfo.Name, serviceInfo.AX); err != nil {
			ts.TsLogAdd(adaptix.LogStatusWarn, 0, "server", "extender", "Service %s: AxScript command catalog failed: %v", serviceInfo.Name, err)
		}
	}

	return nil
}

func (ts *Teamserver) TsServiceUnreg(serviceName string) error {
	if !ts.service_configs.Contains(serviceName) {
		return fmt.Errorf("service %v not found", serviceName)
	}

	if ts.ScriptManager != nil {
		ts.ScriptManager.UnloadServiceScript(serviceName)
	}
	ts.service_configs.Delete(serviceName)

	return nil
}

// Service plugin GUI

func (ts *Teamserver) TsPluginServiceCall(serviceName string, operator string, function string, args string) {
	ts.Extender.ExPluginServiceCall(serviceName, operator, function, args)
}

func (ts *Teamserver) TsPluginServiceCallWait(serviceName string, operator string, function string, args string, timeoutMs int) (string, error) {
	return ts.Extender.ExPluginServiceCallWait(serviceName, operator, function, args, timeoutMs)
}

func (ts *Teamserver) TsPluginServiceSendDataAll(service string, data string) {
	packet := CreateSpPluginServiceData(service, data)
	ts.TsSyncAllClients(packet)
	ts.serviceWaitDeliver("", service, data)
}

func (ts *Teamserver) TsPluginServiceSendDataClient(operator string, service string, data string) {
	packet := CreateSpPluginServiceData(service, data)
	ts.TsSyncClient(operator, packet)
	ts.serviceWaitDeliver(operator, service, data)
}

func serviceWaitKey(operator, service string) string {
	return operator + "\x00" + service
}

func (ts *Teamserver) serviceWaitRegister(operator, service string) chan string {
	ch := make(chan string, 8)
	ts.serviceWaiters.Store(serviceWaitKey(operator, service), ch)
	return ch
}

func (ts *Teamserver) serviceWaitUnregister(operator, service string) {
	ts.serviceWaiters.Delete(serviceWaitKey(operator, service))
}

func (ts *Teamserver) serviceWaitDeliver(operator, service, data string) {
	if operator != "" {
		if v, ok := ts.serviceWaiters.Load(serviceWaitKey(operator, service)); ok {
			select {
			case v.(chan string) <- data:
			default:
			}
		}
	}
	if v, ok := ts.serviceWaiters.Load(serviceWaitKey("", service)); ok {
		select {
		case v.(chan string) <- data:
		default:
		}
	}
}

func (ts *Teamserver) ServiceWaitBegin(operator, service string) (<-chan string, func()) {
	ch := ts.serviceWaitRegister(operator, service)
	cancel := func() { ts.serviceWaitUnregister(operator, service) }
	return ch, cancel
}

// Agent plugin GUI

func (ts *Teamserver) TsPluginAgentCall(agentId int64, operator string, function string, args string) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok || agent == nil {
		ts.pluginAgentSendError(operator, agentId, fmt.Sprintf("agent %d not found", agentId))
		return
	}
	data := agent.GetData()
	agentType := data.Name
	if agentType == "" {
		ts.pluginAgentSendError(operator, agentId, "agent type empty")
		return
	}
	if ts.Extender == nil {
		ts.pluginAgentSendError(operator, agentId, "extender not available")
		return
	}
	ts.Extender.ExPluginAgentCall(agentType, operator, agentId, function, args)
}

func (ts *Teamserver) TsPluginAgentSendDataAll(agentId int64, data string) {
	packet := CreateSpPluginAgentData(agentId, ts.pluginAgentType(agentId), data)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsPluginAgentSendDataClient(operator string, agentId int64, data string) {
	packet := CreateSpPluginAgentData(agentId, ts.pluginAgentType(agentId), data)
	ts.TsSyncClient(operator, packet)
}

func (ts *Teamserver) pluginAgentType(agentId int64) string {
	agent, ok := ts.Agents.Get(agentId)
	if !ok || agent == nil {
		return ""
	}
	return agent.GetData().Name
}

func (ts *Teamserver) pluginAgentSendError(operator string, agentId int64, errMsg string) {
	payload := map[string]any{
		"action":   "error",
		"success":  false,
		"error":    errMsg,
		"agent_id": agentId,
	}
	b, _ := json.Marshal(payload)
	if operator != "" {
		ts.TsPluginAgentSendDataClient(operator, agentId, string(b))
	} else {
		ts.TsPluginAgentSendDataAll(agentId, string(b))
	}
}

// Listener plugin GUI

func (ts *Teamserver) TsPluginListenerCall(listenerName string, operator string, function string, args string) {
	if listenerName == "" {
		ts.pluginListenerSendError(operator, listenerName, "listener name is required")
		return
	}
	ld, ok := ts.listeners.Get(listenerName)
	if !ok {
		ts.pluginListenerSendError(operator, listenerName, fmt.Sprintf("listener %q not found", listenerName))
		return
	}
	listenerType := ld.RegName
	if listenerType == "" {
		ts.pluginListenerSendError(operator, listenerName, "listener type empty")
		return
	}
	if ts.Extender == nil {
		ts.pluginListenerSendError(operator, listenerName, "extender not available")
		return
	}
	ts.Extender.ExPluginListenerCall(listenerType, operator, listenerName, function, args)
}

func (ts *Teamserver) TsPluginListenerSendDataAll(listenerName string, data string) {
	packet := CreateSpPluginListenerData(listenerName, ts.pluginListenerType(listenerName), data)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsPluginListenerSendDataClient(operator string, listenerName string, data string) {
	packet := CreateSpPluginListenerData(listenerName, ts.pluginListenerType(listenerName), data)
	ts.TsSyncClient(operator, packet)
}

func (ts *Teamserver) pluginListenerType(listenerName string) string {
	ld, ok := ts.listeners.Get(listenerName)
	if !ok {
		return ""
	}
	return ld.RegName
}

func (ts *Teamserver) pluginListenerSendError(operator, listenerName, errMsg string) {
	payload := map[string]any{
		"action":   "error",
		"success":  false,
		"error":    errMsg,
		"listener": listenerName,
	}
	b, _ := json.Marshal(payload)
	if operator != "" {
		ts.TsPluginListenerSendDataClient(operator, listenerName, string(b))
	} else {
		ts.TsPluginListenerSendDataAll(listenerName, string(b))
	}
}
