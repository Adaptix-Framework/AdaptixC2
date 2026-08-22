package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
	"fmt"
	"sort"

	adaptix "github.com/Adaptix-Framework/axc2/v2"
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

func (ts *Teamserver) serviceCatalogFor(serviceName string) (adaptix.ServiceCatalogItem, error) {
	if serviceName == "" {
		return adaptix.ServiceCatalogItem{}, fmt.Errorf("service name is required")
	}
	if !ts.service_configs.Contains(serviceName) {
		if ts.Extender == nil || !ts.Extender.ExServiceLoaded(serviceName) {
			return adaptix.ServiceCatalogItem{}, fmt.Errorf("service %q not found", serviceName)
		}
	}
	out := adaptix.ServiceCatalogItem{Name: serviceName, Commands: []adaptix.ServiceCatalogCommand{}}
	if info, ok := ts.service_configs.Get(serviceName); ok {
		out.Title = info.Name
	}
	if ts.Extender != nil {
		out.RPC = ts.Extender.ExServiceHasRPC(serviceName)
	}
	if ts.ScriptManager != nil {
		if g, ok := ts.ScriptManager.ServiceCommandGroup(serviceName); ok {
			if g.GroupName != "" {
				out.Title = g.GroupName
			}
			out.Description = g.GroupDescription
			for _, c := range g.Commands {
				out.Commands = append(out.Commands, adaptix.ServiceCatalogCommand{
					Name:        c.Name,
					Description: c.Description,
					Example:     c.Example,
					Message:     c.Message,
					Destructive: c.Destructive,
				})
			}
		}
	}
	return out, nil
}

func (ts *Teamserver) TsServiceCatalog() (string, error) {
	names := map[string]struct{}{}
	ts.service_configs.ForEachFast(func(n string, _ extender.ServiceInfo) bool {
		names[n] = struct{}{}
		return true
	})
	if ts.Extender != nil {
		for _, n := range ts.Extender.ExServiceList() {
			names[n] = struct{}{}
		}
	}
	rows := make([]adaptix.ServiceCatalogItem, 0, len(names))
	for n := range names {
		cat, err := ts.serviceCatalogFor(n)
		if err != nil {
			continue
		}
		rows = append(rows, cat)
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].Name < rows[j].Name })
	b, err := json.Marshal(rows)
	if err != nil {
		return "", err
	}
	return string(b), nil
}
