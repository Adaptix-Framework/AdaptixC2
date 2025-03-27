package extender

import (
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"plugin"
	"reflect"
)

func NewExtender(teamserver Teamserver) *AdaptixExtender {
	return &AdaptixExtender{
		ts:              teamserver,
		listenerModules: safe.NewMap(),
		agentModules:    safe.NewMap(),
	}
}

func (ex *AdaptixExtender) LoadPlugins(extenderFiles []string) {
	var (
		pl     *plugin.Plugin
		object plugin.Symbol
		err    error
	)

	for _, path := range extenderFiles {

		_, err = os.Stat(path)
		if err != nil {
			logs.Error("", "Plugin %s not found", path)
			continue
		}

		pl, err = plugin.Open(path)
		if err != nil {
			logs.Error("", "Plugin %s did not load: %s", path, err.Error())
			continue
		}

		object, err = pl.Lookup("ModuleObject")
		if err != nil {
			logs.Error("", "Object %s not found in %s", "ModuleObject", path)
			continue
		}

		_, ok := reflect.TypeOf(object).MethodByName("InitPlugin")
		if !ok {
			logs.Error("", "Method %s not found in %s", "InitPlugin", path)
			continue
		}

		buffer, err := object.(CommonFunctions).InitPlugin(ex.ts)
		if err != nil {
			logs.Error("", "InitPlugin %s failed: %s", path, err.Error())
			continue
		}

		var module = new(ModuleExtender)

		err = json.Unmarshal(buffer, &module.Info)
		if err != nil {
			logs.Error("", "InitPlugin JSON Unmarshal error: "+err.Error())
			continue
		}

		err = ex.ValidPlugin(module.Info, object)
		if err != nil {
			logs.Error("", "InitPlugin %s failed: %s", path, err.Error())
			continue
		}

		absPluginPath, err := filepath.Abs(path)
		pluginDir := filepath.Dir(absPluginPath)

		err = ex.ProcessPlugin(module, object, pluginDir)
		if err != nil {
			logs.Error("", "InitPlugin %s failed: %s", path, err.Error())
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

		_, ok = reflect.TypeOf(object).MethodByName("ListenerGetProfile")
		if !ok {
			return errors.New("method ListenerGetProfile not found")
		}
		return nil
	}

	if info.ModuleType == TYPE_AGENT {
		_, ok := reflect.TypeOf(object).MethodByName("AgentInit")
		if !ok {
			return errors.New("method AgentInit not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("AgentGenerate")
		if !ok {
			return errors.New("method AgentGenerate not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("AgentCreate")
		if !ok {
			return errors.New("method AgentCreate not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("AgentProcessData")
		if !ok {
			return errors.New("method AgentProcessData not found")
		}

		_, ok = reflect.TypeOf(object).MethodByName("AgentPackData")
		if !ok {
			return errors.New("method AgentPackData not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentCommand")
		if !ok {
			return errors.New("method AgentCommand not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentDownloadChangeState")
		if !ok {
			return errors.New("method AgentDownloadChangeState not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserDisks")
		if !ok {
			return errors.New("method AgentBrowserDisks not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserProcess")
		if !ok {
			return errors.New("method AgentBrowserProcess not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserFiles")
		if !ok {
			return errors.New("method AgentBrowserFiles not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserUpload")
		if !ok {
			return errors.New("method AgentBrowserUpload not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserDownload")
		if !ok {
			return errors.New("method AgentBrowserDownload not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserJobKill")
		if !ok {
			return errors.New("method AgentBrowserJobKill not found")
		}
		_, ok = reflect.TypeOf(object).MethodByName("AgentBrowserExit")
		if !ok {
			return errors.New("method AgentBrowserExit not found")
		}

		return nil
	}

	return errors.New("unknown modules type: " + info.ModuleType)
}

func (ex *AdaptixExtender) ProcessPlugin(module *ModuleExtender, object plugin.Symbol, pluginPath string) error {

	if module.Info.ModuleType == TYPE_LISTENER {
		buffer, err := object.(ListenerFunctions).ListenerInit(pluginPath, logs.RepoLogsInstance.ListenerPath)
		if err != nil {
			return err
		}

		var listenerInfo ListenerInfo
		err = json.Unmarshal(buffer, &listenerInfo)
		if err != nil {
			logs.Error("", "ProcessPlugin JSON Unmarshal error: "+err.Error())
			return err
		}

		err = ex.ts.TsListenerReg(listenerInfo)
		if err != nil {
			return err
		}

		module.ListenerFunctions = object.(ListenerFunctions)

		listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.Type, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)
		ex.listenerModules.Put(listenerFN, module)

		return nil
	}

	if module.Info.ModuleType == TYPE_AGENT {
		buffer, err := object.(AgentFunctions).AgentInit(pluginPath)
		if err != nil {
			return err
		}

		var agentInfo AgentInfo
		err = json.Unmarshal(buffer, &agentInfo)
		if err != nil {
			logs.Error("", "ProcessPlugin JSON Unmarshal error: "+err.Error())
			return err
		}

		err = ex.ts.TsAgentReg(agentInfo)
		if err != nil {
			return err
		}

		module.AgentFunctions = object.(AgentFunctions)

		ex.agentModules.Put(agentInfo.AgentName, module)

		return nil
	}

	return errors.New("unknown modules type: " + module.Info.ModuleType)
}
