package extender

import "errors"

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentGenerate(config, listenerWM, listenerProfile)
	} else {
		return nil, "", errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCreate(beat)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentProcessData(agentName string, agentObject []byte, packedData []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentProcessData(agentObject, packedData)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentPackData(agentName string, agentObject []byte, maxDataSize int) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentPackData(agentObject, maxDataSize)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentPivotPackData(agentName string, pivotId string, data []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentPivotPackData(pivotId, data)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentCommand(client string, cmdline string, agentName string, agentObject []byte, args map[string]any) error {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCommand(client, cmdline, agentObject, args)
	} else {
		return errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentDownloadChangeState(agentName string, agentObject []byte, newState int, fileId string) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentDownloadChangeState(agentObject, newState, fileId)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserDisks(agentName string, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserDisks(agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserProcess(agentName string, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserProcess(agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserFiles(agentName string, path string, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserFiles(path, agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserUpload(agentName string, path string, content []byte, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserUpload(path, content, agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserDownload(agentName string, path string, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserDownload(path, agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentBrowserJobKill(agentName string, jobId string) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserJobKill(jobId)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentCtxExit(agentName string, agentObject []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentBrowserExit(agentObject)
	} else {
		return nil, errors.New("module not found")
	}
}
