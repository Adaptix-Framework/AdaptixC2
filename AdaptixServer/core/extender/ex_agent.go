package extender

import "errors"

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, "", errors.New("module not found")
	}
	return module.AgentGenerate(config, operatingSystem, listenerWM, listenerProfile)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentCreate(beat)
}

func (ex *AdaptixExtender) ExAgentProcessData(agentName string, agentObject []byte, packedData []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentProcessData(agentObject, packedData)
}

func (ex *AdaptixExtender) ExAgentPackData(agentName string, agentObject []byte, maxDataSize int) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentPackData(agentObject, maxDataSize)
}

func (ex *AdaptixExtender) ExAgentPivotPackData(agentName string, pivotId string, data []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentPivotPackData(pivotId, data)
}

func (ex *AdaptixExtender) ExAgentCommand(client string, cmdline string, agentName string, agentObject []byte, args map[string]any) error {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return errors.New("module not found")
	}
	return module.AgentCommand(client, cmdline, agentObject, args)
}

func (ex *AdaptixExtender) ExAgentDownloadChangeState(agentName string, agentObject []byte, newState int, fileId string) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentDownloadChangeState(agentObject, newState, fileId)
}

func (ex *AdaptixExtender) ExAgentBrowserDisks(agentName string, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserDisks(agentObject)
}

func (ex *AdaptixExtender) ExAgentBrowserProcess(agentName string, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserProcess(agentObject)
}

func (ex *AdaptixExtender) ExAgentBrowserFiles(agentName string, path string, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserFiles(path, agentObject)
}

func (ex *AdaptixExtender) ExAgentBrowserUpload(agentName string, path string, content []byte, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserUpload(path, content, agentObject)
}

func (ex *AdaptixExtender) ExAgentBrowserDownload(agentName string, path string, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserDownload(path, agentObject)
}

func (ex *AdaptixExtender) ExAgentBrowserJobKill(agentName string, jobId string) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserJobKill(jobId)
}

func (ex *AdaptixExtender) ExAgentCtxExit(agentName string, agentObject []byte) ([]byte, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.AgentBrowserExit(agentObject)
}
