package extender

import "errors"

func (ex *AdaptixExtender) ExListenerStart(listenerName string, configType string, config string, listenerCustomData []byte) ([]byte, []byte, error) {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return nil, nil, errors.New("module not found")
	}
	err := module.ListenerValid(config)
	if err != nil {
		return nil, nil, err
	}
	return module.ListenerStart(listenerName, config, listenerCustomData)
}

func (ex *AdaptixExtender) ExListenerStop(listenerName string, configType string) error {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return errors.New("module not found")
	}
	return module.ListenerStop(listenerName)
}

func (ex *AdaptixExtender) ExListenerEdit(listenerName string, configType string, config string) ([]byte, []byte, error) {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return nil, nil, errors.New("module not found")
	}
	return module.ListenerEdit(listenerName, config)
}

func (ex *AdaptixExtender) ExListenerGetProfile(listenerName string, configType string) ([]byte, error) {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return nil, errors.New("module not found")
	}
	return module.ListenerGetProfile(listenerName)
}

func (ex *AdaptixExtender) ExListenerInteralHandler(listenerName string, configType string, data []byte) (string, error) {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return "", errors.New("module not found")
	}
	return module.ListenerInteralHandler(listenerName, data)
}
