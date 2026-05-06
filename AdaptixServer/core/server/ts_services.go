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

func (ts *Teamserver) TsServiceCall(serviceName string, operator string, function string, args string) {
	ts.Extender.ExServiceCall(serviceName, operator, function, args)
}

func (ts *Teamserver) TsServiceList() (string, error) {
	var services []extender.ServiceInfo

	ts.service_configs.ForEach(func(key string, value interface{}) bool {
		serviceInfo := value.(extender.ServiceInfo)
		services = append(services, serviceInfo)
		return true
	})

	jsonServices, err := json.Marshal(services)
	if err != nil {
		return "", err
	}
	return string(jsonServices), nil
}

/// OUT

func (ts *Teamserver) TsServiceSendDataAll(service string, data string) {
	packet := CreateSpServiceData(service, data)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsServiceSendDataClient(operator string, service string, data string) {
	packet := CreateSpServiceData(service, data)
	ts.TsSyncClient(operator, packet)
}
