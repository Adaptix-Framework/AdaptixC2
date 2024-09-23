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

			data, err = module.ListenerStart(config)
			if err != nil {
				return nil, err
			}
		}
	} else {
		return nil, errors.New("module not found")
	}

	return data, nil
}
