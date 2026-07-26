package eventing

import (
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

type EventFilter struct {
	AgentIDs      []int64  `json:"agent_ids,omitempty"`
	AgentNames    []string `json:"agent_names,omitempty"`
	Users         []string `json:"users,omitempty"` // agent username / cred username
	OS            []string `json:"os,omitempty"`    // windows|linux|macos
	Computers     []string `json:"computers,omitempty"`
	Tags          []string `json:"tags,omitempty"` // agent / cred / target tags
	Listeners     []string `json:"listeners,omitempty"`
	ListenerTypes []string `json:"listener_types,omitempty"`
	ListenerTags  []string `json:"listener_tags,omitempty"`
	TaskIDs       []int64  `json:"task_ids,omitempty"`
	Clients       []string `json:"clients,omitempty"`
	FileIDs       []int64  `json:"file_ids,omitempty"`
	Filenames     []string `json:"filenames,omitempty"`
	Ports         []int    `json:"ports,omitempty"`
	TunnelTypes   []string `json:"tunnel_types,omitempty"`
	Realms        []string `json:"realms,omitempty"`
	CredTypes     []string `json:"cred_types,omitempty"`
	Hosts         []string `json:"hosts,omitempty"`
	Domains       []string `json:"domains,omitempty"`
	Addresses     []string `json:"addresses,omitempty"`
	Alive         *bool    `json:"alive,omitempty"`
}

type EventFacts struct {
	Type         string
	AgentID      int64
	AgentName    string
	User         string
	Listener     string
	ListenerType string
	OS           string
	Client       string
	Computer     string
	Tags         string
	TaskID       int64
	FileID       int64
	Filename     string
	Port         int
	TunnelType   string
	Realm        string
	CredType     string
	Host         string
	Domain       string
	Address      string
	Alive        bool
	HasAgent     bool
	HasClient    bool
	HasTask      bool
	HasFile      bool
	HasPort      bool
	HasAlive     bool
}

type FactsEnricher func(facts *EventFacts, event any)

func (f *EventFilter) IsEmpty() bool {
	if f == nil {
		return true
	}
	return len(f.AgentIDs) == 0 && len(f.AgentNames) == 0 && len(f.Users) == 0 &&
		len(f.OS) == 0 && len(f.Computers) == 0 && len(f.Tags) == 0 &&
		len(f.Listeners) == 0 && len(f.ListenerTypes) == 0 && len(f.ListenerTags) == 0 &&
		len(f.TaskIDs) == 0 && len(f.Clients) == 0 &&
		len(f.FileIDs) == 0 && len(f.Filenames) == 0 &&
		len(f.Ports) == 0 && len(f.TunnelTypes) == 0 &&
		len(f.Realms) == 0 && len(f.CredTypes) == 0 && len(f.Hosts) == 0 &&
		len(f.Domains) == 0 && len(f.Addresses) == 0 && f.Alive == nil
}

func (f *EventFilter) Match(facts EventFacts) bool {
	if f == nil || f.IsEmpty() {
		return true
	}
	if !matchI64(f.AgentIDs, facts.HasAgent, facts.AgentID) ||
		!matchStr(f.AgentNames, facts.AgentName) ||
		!matchStr(f.Users, facts.User) ||
		!matchStr(f.OS, facts.OS) ||
		!matchStr(f.Computers, facts.Computer) ||
		!matchTagsDim(f.Tags, facts.Tags) ||
		!matchStr(f.Listeners, facts.Listener) ||
		!matchStr(f.ListenerTypes, facts.ListenerType) ||
		!matchI64(f.TaskIDs, facts.HasTask, facts.TaskID) ||
		!matchStr(f.Clients, facts.Client) ||
		!matchI64(f.FileIDs, facts.HasFile, facts.FileID) ||
		!matchFilename(f.Filenames, facts.Filename) ||
		!matchInt(f.Ports, facts.HasPort, facts.Port) ||
		!matchStr(f.TunnelTypes, facts.TunnelType) ||
		!matchStr(f.Realms, facts.Realm) ||
		!matchStr(f.CredTypes, facts.CredType) ||
		!matchStr(f.Hosts, facts.Host) ||
		!matchStr(f.Domains, facts.Domain) ||
		!matchStr(f.Addresses, facts.Address) ||
		!matchAlive(f.Alive, facts.HasAlive, facts.Alive) {
		return false
	}
	return true
}

func (f *EventFilter) MatchEvent(event any, base EventFacts) bool {
	if f == nil || f.IsEmpty() {
		return true
	}
	switch e := event.(type) {
	case *EventCredentialsAdd:
		if len(e.Credentials) == 0 {
			return f.Match(base)
		}
		for i := range e.Credentials {
			if f.Match(factsFromCred(e.Credentials[i])) {
				return true
			}
		}
		return false
	case *EventCredentialsEdit:
		return f.Match(factsFromCred(e.NewCred)) || f.Match(factsFromCred(e.OldCred))
	case *EventDataTargetAdd:
		if len(e.Targets) == 0 {
			return f.Match(base)
		}
		for i := range e.Targets {
			if f.Match(factsFromTarget(e.Targets[i])) {
				return true
			}
		}
		return false
	case *EventDataTargetEdit:
		return f.Match(factsFromTarget(e.Target))
	default:
		return f.Match(base)
	}
}

func applyAgent(f *EventFacts, a adaptix.AgentData) {
	f.HasAgent = true
	f.AgentID = a.Id
	f.AgentName = a.Name
	f.Listener = a.Listener
	f.OS = osIntToFilterString(a.Os)
	f.Computer = a.Computer
	f.User = a.Username
	if a.Impersonated != "" {
		f.User = a.Impersonated
	}
	f.Tags = a.Tags
}

func factsFromCred(c adaptix.CredsData) EventFacts {
	var f EventFacts
	f.User = c.Username
	f.Realm = c.Realm
	f.CredType = c.Type
	f.Host = c.Host
	f.Tags = c.Tag
	if c.AgentId != 0 {
		f.HasAgent = true
		f.AgentID = c.AgentId
	}
	return f
}

func factsFromTarget(t adaptix.TargetData) EventFacts {
	var f EventFacts
	f.Computer = t.Computer
	f.Domain = t.Domain
	f.Address = t.Address
	f.OS = osIntToFilterString(t.Os)
	f.Tags = t.Tag
	f.HasAlive = true
	f.Alive = t.Alive
	return f
}

func mergeFacts(base, extra EventFacts) EventFacts {
	if extra.HasAgent {
		base.HasAgent = true
		if extra.AgentID != 0 {
			base.AgentID = extra.AgentID
		}
	}
	if extra.AgentName != "" {
		base.AgentName = extra.AgentName
	}
	if extra.User != "" {
		base.User = extra.User
	}
	if extra.OS != "" {
		base.OS = extra.OS
	}
	if extra.Computer != "" {
		base.Computer = extra.Computer
	}
	if extra.Tags != "" {
		base.Tags = extra.Tags
	}
	if extra.Realm != "" {
		base.Realm = extra.Realm
	}
	if extra.CredType != "" {
		base.CredType = extra.CredType
	}
	if extra.Host != "" {
		base.Host = extra.Host
	}
	if extra.Domain != "" {
		base.Domain = extra.Domain
	}
	if extra.Address != "" {
		base.Address = extra.Address
	}
	if extra.HasAlive {
		base.HasAlive = true
		base.Alive = extra.Alive
	}
	return base
}

func osIntToFilterString(os int) string {
	switch os {
	case adaptix.OS_WINDOWS:
		return "windows"
	case adaptix.OS_LINUX:
		return "linux"
	case adaptix.OS_MAC:
		return "macos"
	default:
		return ""
	}
}

func basenamePath(p string) string {
	p = strings.TrimSpace(p)
	if p == "" {
		return ""
	}
	slash := strings.LastIndexByte(p, '/')
	bslash := strings.LastIndexByte(p, '\\')
	i := slash
	if bslash > i {
		i = bslash
	}
	if i >= 0 && i+1 < len(p) {
		return p[i+1:]
	}
	return p
}

func matchI64(want []int64, has bool, v int64) bool {
	if len(want) == 0 || !has {
		return true
	}
	return containsI64(want, v)
}

func matchInt(want []int, has bool, v int) bool {
	if len(want) == 0 || !has {
		return true
	}
	return containsInt(want, v)
}

func matchStr(want []string, v string) bool {
	if len(want) == 0 {
		return true
	}
	v = strings.TrimSpace(v)
	if v == "" {
		return true
	}
	return containsExact(want, strings.ToLower(v))
}

func matchFilename(want []string, name string) bool {
	if len(want) == 0 {
		return true
	}
	name = strings.TrimSpace(name)
	if name == "" {
		return true
	}
	low := strings.ToLower(name)
	if containsExact(want, low) {
		return true
	}
	for _, n := range want {
		if n != "" && strings.Contains(low, n) {
			return true
		}
	}
	return false
}

func matchTagsDim(want []string, factTags string) bool {
	if len(want) == 0 {
		return true
	}
	factTags = strings.TrimSpace(factTags)
	if factTags == "" {
		return true
	}
	return matchTags(factTags, want)
}

func matchAlive(want *bool, has, v bool) bool {
	if want == nil || !has {
		return true
	}
	return v == *want
}

func containsI64(list []int64, v int64) bool {
	for _, x := range list {
		if x == v {
			return true
		}
	}
	return false
}

func containsInt(list []int, v int) bool {
	for _, x := range list {
		if x == v {
			return true
		}
	}
	return false
}

func containsExact(list []string, vLower string) bool {
	for _, x := range list {
		if x == vLower {
			return true
		}
	}
	return false
}

func matchTags(factTags string, want []string) bool {
	if len(want) == 0 {
		return true
	}
	factLow := strings.ToLower(factTags)
	tokens := splitTagsLower(factTags)
	if len(tokens) == 0 {
		for _, w := range want {
			if w != "" && strings.Contains(factLow, w) {
				return true
			}
		}
		return false
	}
	for _, w := range want {
		if w == "" {
			continue
		}
		for _, t := range tokens {
			if t == w {
				return true
			}
		}
		if strings.Contains(factLow, w) {
			return true
		}
	}
	return false
}

func splitTagsLower(s string) []string {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil
	}
	var out []string
	start := -1
	flush := func(end int) {
		if start < 0 {
			return
		}
		tok := strings.TrimSpace(s[start:end])
		if tok != "" {
			out = append(out, strings.ToLower(tok))
		}
		start = -1
	}
	for i := 0; i < len(s); i++ {
		c := s[i]
		sep := c == ',' || c == ';' || c == '|' || c == '\t' || c == ' '
		if sep {
			flush(i)
			continue
		}
		if start < 0 {
			start = i
		}
	}
	flush(len(s))
	return out
}

func NormalizeFilter(f *EventFilter) *EventFilter {
	if f == nil || f.IsEmpty() {
		return nil
	}
	out := &EventFilter{
		AgentIDs:      uniqueI64(f.AgentIDs),
		AgentNames:    uniqueLower(f.AgentNames),
		Users:         uniqueLower(f.Users),
		OS:            uniqueLower(normalizeOSList(f.OS)),
		Computers:     uniqueLower(f.Computers),
		Tags:          uniqueLower(f.Tags),
		Listeners:     uniqueLower(f.Listeners),
		ListenerTypes: uniqueLower(f.ListenerTypes),
		ListenerTags:  uniqueLower(f.ListenerTags),
		TaskIDs:       uniqueI64(f.TaskIDs),
		Clients:       uniqueLower(f.Clients),
		FileIDs:       uniqueI64(f.FileIDs),
		Filenames:     uniqueLower(f.Filenames),
		Ports:         uniqueInt(f.Ports),
		TunnelTypes:   uniqueLower(f.TunnelTypes),
		Realms:        uniqueLower(f.Realms),
		CredTypes:     uniqueLower(f.CredTypes),
		Hosts:         uniqueLower(f.Hosts),
		Domains:       uniqueLower(f.Domains),
		Addresses:     uniqueLower(f.Addresses),
		Alive:         f.Alive,
	}
	if out.IsEmpty() {
		return nil
	}
	return out
}

func normalizeOSList(list []string) []string {
	if len(list) == 0 {
		return nil
	}
	out := make([]string, 0, len(list))
	for _, s := range list {
		s = strings.ToLower(strings.TrimSpace(s))
		switch s {
		case "win", "windows":
			out = append(out, "windows")
		case "linux":
			out = append(out, "linux")
		case "mac", "macos", "darwin", "osx":
			out = append(out, "macos")
		case "":
			continue
		default:
			out = append(out, s)
		}
	}
	return out
}

func uniqueI64(in []int64) []int64 {
	if len(in) == 0 {
		return nil
	}
	if len(in) == 1 {
		return []int64{in[0]}
	}
	seen := make(map[int64]struct{}, len(in))
	out := make([]int64, 0, len(in))
	for _, v := range in {
		if _, ok := seen[v]; ok {
			continue
		}
		seen[v] = struct{}{}
		out = append(out, v)
	}
	return out
}

func uniqueInt(in []int) []int {
	if len(in) == 0 {
		return nil
	}
	if len(in) == 1 {
		return []int{in[0]}
	}
	seen := make(map[int]struct{}, len(in))
	out := make([]int, 0, len(in))
	for _, v := range in {
		if _, ok := seen[v]; ok {
			continue
		}
		seen[v] = struct{}{}
		out = append(out, v)
	}
	return out
}

func uniqueLower(in []string) []string {
	if len(in) == 0 {
		return nil
	}
	seen := make(map[string]struct{}, len(in))
	out := make([]string, 0, len(in))
	for _, s := range in {
		s = strings.ToLower(strings.TrimSpace(s))
		if s == "" {
			continue
		}
		if _, ok := seen[s]; ok {
			continue
		}
		seen[s] = struct{}{}
		out = append(out, s)
	}
	if len(out) == 0 {
		return nil
	}
	return out
}

func ParseAgentIDList(raw []interface{}) []int64 {
	out := make([]int64, 0, len(raw))
	for _, v := range raw {
		switch t := v.(type) {
		case float64:
			out = append(out, int64(t))
		case int64:
			out = append(out, t)
		case int:
			out = append(out, int64(t))
		case string:
			if n, err := strconv.ParseInt(strings.TrimSpace(t), 10, 64); err == nil {
				out = append(out, n)
			}
		}
	}
	return out
}
