package extender

import (
	"errors"
	"github.com/Adaptix-Framework/axc2"
)

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, "", errors.New("module not found")
	}
	return module.AgentGenerate(config, listenerWM, listenerProfile)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) (adaptix.AgentData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.AgentData{}, errors.New("module not found")
	}
	return module.AgentCreate(beat)
}

func (ex *AdaptixExtender) ExAgentCommand(agentName string, agentData adaptix.AgentData, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.TaskData{}, adaptix.ConsoleMessageData{}, errors.New("module not found")
	}
	return module.AgentCommand(agentData, args)
}

func (ex *AdaptixExtender) ExAgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentProcessData(agentData, packedData)
}

func (ex *AdaptixExtender) ExAgentPackData(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentPackData(agentData, tasks)
}

func (ex *AdaptixExtender) ExAgentPivotPackData(agentName string, pivotId string, data []byte) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentPivotPackData(pivotId, data)
}

/// Tunnels

func (ex *AdaptixExtender) ExAgentTunnelCallbacks(agentData adaptix.AgentData, tunnelType int) (func(channelId int, address string, port int) adaptix.TaskData, func(channelId int, address string, port int) adaptix.TaskData, func(channelId int, data []byte) adaptix.TaskData, func(channelId int, data []byte) adaptix.TaskData, func(channelId int) adaptix.TaskData, func(tunnelId int, port int) adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, nil, nil, nil, nil, nil, errors.New("module not found")
	}
	return module.AgentTunnelCallbacks()
}

func (ex *AdaptixExtender) ExAgentTerminalCallbacks(agentData adaptix.AgentData) (func(int, string, int, int) (adaptix.TaskData, error), func(int, []byte) (adaptix.TaskData, error), func(int) (adaptix.TaskData, error), error) {

	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, nil, nil, errors.New("module not found")
	}
	return module.AgentTerminalCallbacks()
}

////

func (ex *AdaptixExtender) ExAgentBrowserJobKill(agentData adaptix.AgentData, jobId string) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserJobKill(jobId)
}
