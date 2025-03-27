package extender

import "errors"

func (ex *AdaptixExtender) ExListenerStart(listenerName string, configType string, config string, listenerCustomData []byte) ([]byte, []byte, error) {
	var (
		err    error
		module *ModuleExtender
	)

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		err = module.ListenerValid(config)
		if err != nil {
			return nil, nil, err
		}
		return module.ListenerStart(listenerName, config, listenerCustomData)

	} else {
		return nil, nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExListenerStop(listenerName string, configType string) error {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerStop(listenerName)

	} else {
		return errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExListenerEdit(listenerName string, configType string, config string) ([]byte, []byte, error) {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerEdit(listenerName, config)

	} else {
		return nil, nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExListenerGetProfile(listenerName string, configType string) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerGetProfile(listenerName)

	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExListenerInteralHandler(listenerName string, configType string, data []byte) (string, error) {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerInteralHandler(listenerName, data)
	} else {
		return "", errors.New("module not found")
	}
}
