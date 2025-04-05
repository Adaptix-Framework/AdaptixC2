package extender

import (
	"errors"
	"github.com/Adaptix-Framework/axc2"
)

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, "", errors.New("module not found")
	}
	return module.AgentGenerate(config, operatingSystem, listenerWM, listenerProfile)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) (adaptix.AgentData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.AgentData{}, errors.New("module not found")
	}
	return module.AgentCreate(beat)
}

func (ex *AdaptixExtender) ExAgentCommand(client string, cmdline string, agentName string, agentData adaptix.AgentData, args map[string]any) error {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return errors.New("module not found")
	}
	return module.AgentCommand(client, cmdline, agentData, args)
}

func (ex *AdaptixExtender) ExAgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentProcessData(agentData, packedData)
}

func (ex *AdaptixExtender) ExAgentPackData(agentData adaptix.AgentData, maxDataSize int) ([]byte, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentPackData(agentData, maxDataSize)
}

func (ex *AdaptixExtender) ExAgentPivotPackData(agentName string, pivotId string, data []byte) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentPivotPackData(pivotId, data)
}

func (ex *AdaptixExtender) ExAgentDownloadChangeState(agentData adaptix.AgentData, newState int, fileId string) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentDownloadChangeState(agentData, newState, fileId)
}

func (ex *AdaptixExtender) ExAgentBrowserDisks(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserDisks(agentData)
}

func (ex *AdaptixExtender) ExAgentBrowserProcess(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserProcess(agentData)
}

func (ex *AdaptixExtender) ExAgentBrowserFiles(agentData adaptix.AgentData, path string) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserFiles(path, agentData)
}

func (ex *AdaptixExtender) ExAgentBrowserUpload(agentData adaptix.AgentData, path string, content []byte) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserUpload(path, content, agentData)
}

func (ex *AdaptixExtender) ExAgentBrowserDownload(agentData adaptix.AgentData, path string) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserDownload(path, agentData)
}

func (ex *AdaptixExtender) ExAgentCtxExit(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentData.Name]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserExit(agentData)
}

func (ex *AdaptixExtender) ExAgentBrowserJobKill(agentName string, jobId string) (adaptix.TaskData, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return adaptix.TaskData{}, errors.New("module not found")
	}
	return module.AgentBrowserJobKill(jobId)
}
