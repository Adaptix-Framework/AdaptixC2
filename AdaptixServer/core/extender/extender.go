package extender

import (
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"plugin"
	"reflect"
	"strings"
)

func NewExtender(teamserver Teamserver) *AdaptixExtender {
	return &AdaptixExtender{
		ts:              teamserver,
		listenerModules: safe.NewMap(),
	}
}

func (ex *AdaptixExtender) LoadPlugins(extenderFile string) {
	var (
		pl     *plugin.Plugin
		object plugin.Symbol
		err    error

		module = new(ModuleExtender)
	)

	extenderContent, err := os.ReadFile(extenderFile)
	if err != nil {
		logs.Error("File %s not read", extenderFile)
		return
	}

	files := strings.Split(string(extenderContent), "\n")
	for _, path := range files {

		_, err = os.Stat(path)
		if err != nil {
			logs.Error("Plugin %s not found", path)
			continue
		}

		pl, err = plugin.Open(path)
		if err != nil {
			logs.Error("Plugin %s did not load", path)
			continue
		}

		object, err = pl.Lookup("ModuleObject")
		if err != nil {
			logs.Error("Object %s not found in %s", "ModuleObject", path)
			continue
		}

		_, ok := reflect.TypeOf(object).MethodByName("InitPlugin")
		if !ok {
			logs.Error("Method %s not found in %s", "InitPlugin", path)
			continue
		}

		buffer, err := object.(CommonFunctions).InitPlugin(ex.ts)
		if err != nil {
			logs.Error("InitPlugin %s failed: %s", path, err.Error())
			continue
		}

		err = json.Unmarshal(buffer, &module.Info)
		if err != nil {
			logs.Error("InitPlugin JSON Unmarshal error: " + err.Error())
			continue
		}

		err = ex.ValidPlugin(module.Info, object)
		if err != nil {
			logs.Error("InitPlugin %s failed: %s", path, err.Error())
			continue
		}

		err = ex.ProcessPlugin(module, object)
		if err != nil {
			logs.Error("InitPlugin %s failed: %s", path, err.Error())
			continue
		}
	}
}

func (ex *AdaptixExtender) ValidPlugin(info ModuleInfo, object plugin.Symbol) error {
	if info.ModuleType == TYPE_LISTENER {

		_, ok := reflect.TypeOf(object).MethodByName("ListenerInit")
		if !ok {
			return errors.New("method ListenerInit not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("ListenerStart")
		if !ok {
			return errors.New("method ListenerStart not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("ListenerValid")
		if !ok {
			return errors.New("method ListenerValid not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("ListenerStop")
		if !ok {
			return errors.New("method ListenerStop not found")
		}

		return nil
	}

	return errors.New("unknown modules type: " + info.ModuleType)
}

func (ex *AdaptixExtender) ProcessPlugin(module *ModuleExtender, object plugin.Symbol) error {
	if module.Info.ModuleType == TYPE_LISTENER {
		buffer, err := object.(ListenerFunctions).ListenerInit()
		if err != nil {
			return err
		}

		var listenerInfo ListenerInfo
		err = json.Unmarshal(buffer, &listenerInfo)
		if err != nil {
			logs.Error("ProcessPlugin JSON Unmarshal error: " + err.Error())
			return err
		}

		err = ex.ts.ListenerNew(listenerInfo)
		if err != nil {
			return err
		}

		module.ListenerFunctions = object.(ListenerFunctions)

		listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.ListenerType, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)
		ex.listenerModules.Put(listenerFN, module)

		return nil
	}

	return errors.New("unknown modules type: " + module.Info.ModuleType)
}
