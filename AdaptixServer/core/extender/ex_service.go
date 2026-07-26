package extender

import (
	"fmt"
	"os"
	"path/filepath"
	"plugin"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/goccy/go-yaml"
)

func (ex *AdaptixExtender) ExServiceLoad(configPath string) error {
	_, err := os.Stat(configPath)
	if err != nil {
		return err
	}

	configData, err := os.ReadFile(configPath)
	if err != nil {
		return err
	}

	var configService ExConfigService
	err = yaml.Unmarshal(configData, &configService)
	if err != nil {
		return err
	}

	if ex.serviceModules.Contains(configService.ServiceName) {
		return ErrServiceAlreadyLoaded
	}

	pluginPath := filepath.Dir(configPath) + "/" + configService.ExtenderFile
	plug, err := plugin.Open(pluginPath)
	if err != nil {
		return err
	}

	sym, err := plug.Lookup("InitPlugin")
	if err != nil {
		return err
	}

	plInitPlugin, ok := sym.(func(ts any, moduleDir string, serviceConfig string) adaptix.PluginService)
	if !ok {
		return fmt.Errorf("unexpected InitPlugin signature in %s", pluginPath)
	}

	plService := plInitPlugin(ex.ts, filepath.Dir(pluginPath), configService.ServiceConfig)
	if plService == nil {
		return fmt.Errorf("InitPlugin returned nil for %s", pluginPath)
	}

	serviceInfo := ServiceInfo{
		Name: configService.ServiceName,
	}

	if configService.AxFile != "" {
		axPath := filepath.Dir(configPath) + "/" + configService.AxFile
		axContent, err := os.ReadFile(axPath)
		if err != nil {
			ex.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "server", "extender", "failed to read ax file %s: %s", axPath, err.Error())
		} else {
			serviceInfo.AX = string(axContent)
		}
	}

	err = ex.ts.TsServiceReg(serviceInfo)
	if err != nil {
		return err
	}

	ex.serviceModules.Put(serviceInfo.Name, plService)
	ex.ts.TsLogAdd(adaptix.LogStatusSuccess, 0, "server", "extender", "Service '%s' loaded", configService.ServiceName)
	return nil
}

func (ex *AdaptixExtender) ExServiceUnload(serviceName string) error {
	if !ex.serviceModules.Contains(serviceName) {
		return ErrServiceNotFound
	}

	err := ex.ts.TsServiceUnreg(serviceName)
	if err != nil {
		return err
	}

	ex.serviceModules.Delete(serviceName)
	ex.ts.TsLogAdd(adaptix.LogStatusSuccess, 0, "server", "extender", "Service '%s' unloaded", serviceName)
	return nil
}

func (ex *AdaptixExtender) ExServiceCall(serviceName string, operator string, function string, args string) {
	module, err := ex.getServiceModule(serviceName)
	if err == nil {
		module.Call(operator, function, args)
	}
}

func (ex *AdaptixExtender) ExServiceList() []string {
	var services []string
	ex.serviceModules.ForEachFast(func(name string, _ adaptix.PluginService) bool {
		services = append(services, name)
		return true
	})
	return services
}
