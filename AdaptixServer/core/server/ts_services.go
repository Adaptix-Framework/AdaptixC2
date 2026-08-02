package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
)

func (ts *Teamserver) TsServiceLoad(configPath string) error {
	return ts.Extender.ExServiceLoad(configPath)
}

func (ts *Teamserver) TsServiceUnload(serviceName string) error {
	return ts.Extender.ExServiceUnload(serviceName)
}

func (ts *Teamserver) TsServiceList() (string, error) {
	var services []extender.ServiceInfo

	ts.service_configs.ForEachFast(func(key string, serviceInfo extender.ServiceInfo) bool {
		services = append(services, serviceInfo)
		return true
	})

	jsonServices, err := json.Marshal(services)
	if err != nil {
		return "", err
	}
	return string(jsonServices), nil
}
