package spec

import "strings"

type HostDeps struct {
	Apt []string `yaml:"apt,omitempty"`
}

type ProjectDeps struct {
	Common HostDeps `yaml:"common,omitempty"`
	Server HostDeps `yaml:"server,omitempty"`
	Client HostDeps `yaml:"client,omitempty"`
}

func (d ProjectDeps) AptPackages(server, client bool) []string {
	var out []string
	out = append(out, d.Common.Apt...)
	if server {
		out = append(out, d.Server.Apt...)
	}
	if client {
		out = append(out, d.Client.Apt...)
	}
	return uniqueNonEmpty(out)
}

func (h HostDeps) AptPackages() []string {
	return uniqueNonEmpty(h.Apt)
}

func (p PluginSpec) CollectPluginAptDeps() []string {
	var out []string
	for _, e := range p.Extenders {
		out = append(out, e.Deps.Apt...)
	}

	return uniqueNonEmpty(out)
}

func uniqueNonEmpty(in []string) []string {
	seen := make(map[string]struct{}, len(in))
	out := make([]string, 0, len(in))
	for _, s := range in {
		s = strings.TrimSpace(s)
		if s == "" {
			continue
		}
		if _, ok := seen[s]; ok {
			continue
		}
		seen[s] = struct{}{}
		out = append(out, s)
	}
	return out
}
