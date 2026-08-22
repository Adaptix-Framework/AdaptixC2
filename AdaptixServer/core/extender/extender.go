package extender

import (
	"os"
	"path/filepath"
	"plugin"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/Adaptix-Framework/axsafe"
	"github.com/goccy/go-yaml"
)

func NewExtender(teamserver Teamserver, listenerPath string) *AdaptixExtender {
	return &AdaptixExtender{
		ts:              teamserver,
		listenerPath:    listenerPath,
		listenerModules: axsafe.NewMap[string, adaptix.PluginListener](),
		agentModules:    axsafe.NewMap[string, adaptix.PluginAgent](),
		serviceModules:  axsafe.NewMap[string, adaptix.PluginService](),
		activeListeners: axsafe.NewMap[string, adaptix.ExtenderListener](),
	}
}

func (ex *AdaptixExtender) LoadPlugins(extenderFiles []string) {

	for _, path := range extenderFiles {

		_, err := os.Stat(path)
		if err != nil {
			ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Config %s not found", path)
			continue
		}
		config_data, err := os.ReadFile(path)
		if err != nil {
			ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Read file %s error: %s", path, err.Error())
			continue
		}

		var config_map map[string]any
		err = yaml.Unmarshal(config_data, &config_map)
		if err != nil {
			ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Error config %s parse: %s", path, err.Error())
			continue
		}
		extender_type, ok_type := config_map["extender_type"].(string)
		if !ok_type {
			ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Error config %s parse: extender_type not found", path)
			continue
		}

		if extender_type == "listener" {
			ex.LoadPluginListener(path, config_data)
		} else if extender_type == "agent" {
			ex.LoadPluginAgent(path, config_data)
		} else if extender_type == "service" {
			ex.LoadPluginService(path, config_data)
		} else {
			ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Unknown extender_type in %s", path)
		}
	}
}

func (ex *AdaptixExtender) LoadPluginListener(config_path string, config_data []byte) {
	var configListener ExConfigListener
	err := yaml.Unmarshal(config_data, &configListener)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Error config parse: %s", err.Error())
		return
	}

	plugin_path := filepath.Dir(config_path) + "/" + configListener.ExtenderFile
	plug, err := plugin.Open(plugin_path)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to open plugin %s: %s", plugin_path, err.Error())
		return
	}

	sym, err := plug.Lookup("InitPlugin")
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to find InitPlugin in %s: %s", plugin_path, err.Error())
		return
	}

	pl_InitPlugin, ok := sym.(func(ts any, moduleDir string, listenerDir string) adaptix.PluginListener)
	if !ok {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "unexpected signature from InitPlugin in %s", plugin_path)
		return
	}

	pl_listener := pl_InitPlugin(ex.ts, filepath.Dir(plugin_path), ex.listenerPath)
	if pl_listener == nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s returned nil", plugin_path)
		return
	}

	ax_path := filepath.Dir(config_path) + "/" + configListener.AxFile
	ax_content, err := os.ReadFile(ax_path)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to read ax file %s: %s", ax_path, err.Error())
		return
	}

	listenerInfo := ListenerInfo{
		Name:     configListener.ListenerName,
		Type:     configListener.ListenerType,
		Protocol: configListener.Protocol,
		AX:       string(ax_content),
	}

	err = ex.ts.TsListenerReg(listenerInfo)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s does not registered: %s", plugin_path, err.Error())
		return
	}

	ex.listenerModules.Put(listenerInfo.Name, pl_listener)
}

func (ex *AdaptixExtender) LoadPluginAgent(config_path string, config_data []byte) {
	var configAgent ExConfigAgent
	err := yaml.Unmarshal(config_data, &configAgent)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Error config parse: %s", err.Error())
		return
	}

	ax_path := filepath.Dir(config_path) + "/" + configAgent.AxFile
	ax_content, err := os.ReadFile(ax_path)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to read ax file %s: %s", ax_path, err.Error())
		return
	}

	agentInfo := AgentInfo{
		Name:           configAgent.AgentName,
		Watermark:      configAgent.AgentWatermark,
		AX:             string(ax_content),
		Listeners:      configAgent.Listeners,
		MultiListeners: configAgent.MultiListeners,
	}

	plugin_path := filepath.Dir(config_path) + "/" + configAgent.ExtenderFile
	plug, err := plugin.Open(plugin_path)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to open plugin %s: %s", plugin_path, err.Error())
		return
	}

	sym, err := plug.Lookup("InitPlugin")
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to find InitPlugin in %s: %s", plugin_path, err.Error())
		return
	}

	pl_InitPlugin, ok := sym.(func(ts any, moduleDir string, watermark string) adaptix.PluginAgent)
	if !ok {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "unexpected signature from InitPlugin in %s", plugin_path)
		return
	}

	pl_agent := pl_InitPlugin(ex.ts, filepath.Dir(plugin_path), agentInfo.Watermark)
	if pl_agent == nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s returned nil", plugin_path)
		return
	}

	err = ex.ts.TsAgentReg(agentInfo)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s does not registered: %s", plugin_path, err.Error())
		return
	}

	ex.agentModules.Put(agentInfo.Name, pl_agent)
}

func (ex *AdaptixExtender) LoadPluginService(config_path string, config_data []byte) {
	var configService ExConfigService
	err := yaml.Unmarshal(config_data, &configService)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "Error config parse: %s", err.Error())
		return
	}

	plugin_path := filepath.Dir(config_path) + "/" + configService.ExtenderFile
	plug, err := plugin.Open(plugin_path)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to open plugin %s: %s", plugin_path, err.Error())
		return
	}

	sym, err := plug.Lookup("InitPlugin")
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "failed to find InitPlugin in %s: %s", plugin_path, err.Error())
		return
	}

	pl_InitPlugin, ok := sym.(func(ts any, moduleDir string, serviceConfig string) adaptix.PluginService)
	if !ok {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "unexpected signature from InitPlugin in %s", plugin_path)
		return
	}

	pl_service := pl_InitPlugin(ex.ts, filepath.Dir(plugin_path), configService.ServiceConfig)
	if pl_service == nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s returned nil", plugin_path)
		return
	}

	serviceInfo := ServiceInfo{
		Name: configService.ServiceName,
	}

	if configService.AxFile != "" {
		ax_path := filepath.Dir(config_path) + "/" + configService.AxFile
		ax_content, err := os.ReadFile(ax_path)
		if err != nil {
			ex.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "server", "extender_manager", "failed to read ax file %s: %s", ax_path, err.Error())
		} else {
			serviceInfo.AX = string(ax_content)
		}
	}

	err = ex.ts.TsServiceReg(serviceInfo)
	if err != nil {
		ex.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "extender_manager", "plugin %s does not registered: %s", plugin_path, err.Error())
		return
	}

	ex.serviceModules.Put(serviceInfo.Name, pl_service)

	ex.ts.TsLogAdd(adaptix.LogStatusSuccess, 0, "server", "extender_manager", "Service '%s' loaded", configService.ServiceName)
}
