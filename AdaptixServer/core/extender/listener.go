package extender

import "errors"

func (ex *AdaptixExtender) ListenerStart(listenerName string, configType string, config string) ([]byte, error) {
	var (
		err    error
		data   []byte
		module *ModuleExtender
	)

	if ex.listenerModules.Contains(configType) {
		value, ok := ex.listenerModules.Get(configType)
		if ok {
			module = value.(*ModuleExtender)
			err = module.ListenerValid(config)
			if err != nil {
				return nil, err
			}

			data, err = module.ListenerStart(listenerName, config)
			if err != nil {
				return nil, err
			}
		}
	} else {
		return nil, errors.New("module not found")
	}

	return data, nil
}

func (ex *AdaptixExtender) ListenerStop(listenerName string, configType string) error {
	var (
		err    error
		module *ModuleExtender
	)

	if ex.listenerModules.Contains(configType) {
		value, ok := ex.listenerModules.Get(configType)
		if ok {
			module = value.(*ModuleExtender)
			err = module.ListenerStop(listenerName)
			return err
		}
	} else {
		return errors.New("module not found")
	}

	return nil
}

func (ex *AdaptixExtender) ListenerEdit(listenerName string, configType string, config string) ([]byte, error) {
	var (
		err    error
		data   []byte
		module *ModuleExtender
	)

	if ex.listenerModules.Contains(configType) {
		value, ok := ex.listenerModules.Get(configType)
		if ok {
			module = value.(*ModuleExtender)
			data, err = module.ListenerEdit(listenerName, config)
			if err != nil {
				return nil, err
			}
		}
	} else {
		return nil, errors.New("module not found")
	}

	return data, nil
}
