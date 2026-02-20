package axscript

import (
	"fmt"
	"sync"
)

type SourceType int

const (
	SourceAgent SourceType = iota
	SourceProfile
	SourceUser
)

func (s SourceType) String() string {
	switch s {
	case SourceAgent:
		return "agent"
	case SourceProfile:
		return "profile"
	case SourceUser:
		return "user"
	default:
		return "unknown"
	}
}

type CommandStore struct {
	mu sync.RWMutex

	AgentRegistry   *CommandRegistry
	ProfileRegistry *CommandRegistry
	UserRegistry    *CommandRegistry
}

func NewCommandStore() *CommandStore {
	return &CommandStore{
		AgentRegistry:   NewCommandRegistry(),
		ProfileRegistry: NewCommandRegistry(),
		UserRegistry:    NewCommandRegistry(),
	}
}

func (cs *CommandStore) GetRegistry(source SourceType) *CommandRegistry {
	switch source {
	case SourceAgent:
		return cs.AgentRegistry
	case SourceProfile:
		return cs.ProfileRegistry
	case SourceUser:
		return cs.UserRegistry
	default:
		return nil
	}
}

func (cs *CommandStore) RegisterGroups(source SourceType, agentName string, listener string, os int, groups []CommandGroup, engine *ScriptEngine) {
	registry := cs.GetRegistry(source)
	if registry != nil {
		registry.RegisterGroups(agentName, listener, os, groups, engine)
	}
}

// /---
func (cs *CommandStore) UnregisterByScriptName(source SourceType, scriptName string) {
	registry := cs.GetRegistry(source)
	if registry != nil {
		registry.UnregisterByScriptName(scriptName)
	}
}

func (cs *CommandStore) Resolve(agentName string, listener string, os int, commandName string) *ResolvedCommand {
	if res := cs.AgentRegistry.Resolve(agentName, listener, os, commandName); res != nil {
		return res
	}
	if res := cs.ProfileRegistry.Resolve(agentName, listener, os, commandName); res != nil {
		return res
	}
	if res := cs.UserRegistry.Resolve(agentName, listener, os, commandName); res != nil {
		return res
	}
	return nil
}

func (cs *CommandStore) ResolveSubcommand(agentName string, listener string, os int, commandName string, subcommandName string) *ResolvedCommand {
	if res := cs.AgentRegistry.ResolveSubcommand(agentName, listener, os, commandName, subcommandName); res != nil {
		return res
	}
	if res := cs.ProfileRegistry.ResolveSubcommand(agentName, listener, os, commandName, subcommandName); res != nil {
		return res
	}
	if res := cs.UserRegistry.ResolveSubcommand(agentName, listener, os, commandName, subcommandName); res != nil {
		return res
	}
	return nil
}

func (cs *CommandStore) ResolveFromCmdline(agentName string, listener string, os int, cmdline string) (*ResolvedCommand, error) {
	tokens := Tokenize(cmdline)
	if len(tokens) == 0 {
		return nil, fmt.Errorf("empty command line")
	}

	commandName := tokens[0]
	resolved := cs.Resolve(agentName, listener, os, commandName)
	if resolved == nil {
		return nil, fmt.Errorf("command '%s' not found for agent '%s' os=%d", commandName, agentName, os)
	}

	if len(resolved.Command.Subcommands) > 0 {
		if len(tokens) < 2 {
			return nil, fmt.Errorf("subcommand required for '%s'", commandName)
		}
		subName := tokens[1]
		resolved = cs.ResolveSubcommand(agentName, listener, os, commandName, subName)
		if resolved == nil {
			return nil, fmt.Errorf("subcommand '%s' not found for command '%s'", subName, commandName)
		}
	}

	return resolved, nil
}

// /---
func (cs *CommandStore) GetCommandsForAgent(agentName string, listener string, os int) []CommandGroup {
	var result []CommandGroup
	result = append(result, cs.AgentRegistry.GetCommandsForAgent(agentName, listener, os)...)
	result = append(result, cs.ProfileRegistry.GetCommandsForAgent(agentName, listener, os)...)
	result = append(result, cs.UserRegistry.GetCommandsForAgent(agentName, listener, os)...)
	return result
}

// /---
func (cs *CommandStore) GetAllCommandsOrdered() []CommandBatch {
	var result []CommandBatch

	for agent, listenerMap := range cs.AgentRegistry.GetAllCommands() {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				if len(groups) > 0 {
					result = append(result, CommandBatch{
						Source:   SourceAgent,
						Agent:    agent,
						Listener: listener,
						Os:       osType,
						Groups:   groups,
					})
				}
			}
		}
	}

	for agent, listenerMap := range cs.ProfileRegistry.GetAllCommands() {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				if len(groups) > 0 {
					result = append(result, CommandBatch{
						Source:   SourceProfile,
						Agent:    agent,
						Listener: listener,
						Os:       osType,
						Groups:   groups,
					})
				}
			}
		}
	}

	for agent, listenerMap := range cs.UserRegistry.GetAllCommands() {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				if len(groups) > 0 {
					result = append(result, CommandBatch{
						Source:   SourceUser,
						Agent:    agent,
						Listener: listener,
						Os:       osType,
						Groups:   groups,
					})
				}
			}
		}
	}

	return result
}

func (cs *CommandStore) GetProfileAndUserCommands() []CommandBatch {
	var result []CommandBatch

	for agent, listenerMap := range cs.ProfileRegistry.GetAllCommands() {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				if len(groups) > 0 {
					result = append(result, CommandBatch{
						Source:   SourceProfile,
						Agent:    agent,
						Listener: listener,
						Os:       osType,
						Groups:   groups,
					})
				}
			}
		}
	}

	for agent, listenerMap := range cs.UserRegistry.GetAllCommands() {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				if len(groups) > 0 {
					result = append(result, CommandBatch{
						Source:   SourceUser,
						Agent:    agent,
						Listener: listener,
						Os:       osType,
						Groups:   groups,
					})
				}
			}
		}
	}

	return result
}

func (cs *CommandStore) GetAgentCommandBatches(agentName string) []CommandBatch {
	var result []CommandBatch

	listenerMap, exists := cs.AgentRegistry.GetAllCommands()[agentName]
	if !exists {
		return result
	}

	for listener, osMap := range listenerMap {
		for osType, groups := range osMap {
			if len(groups) > 0 {
				result = append(result, CommandBatch{
					Source:   SourceAgent,
					Agent:    agentName,
					Listener: listener,
					Os:       osType,
					Groups:   groups,
				})
			}
		}
	}

	return result
}

type CommandBatch struct {
	Source   SourceType
	Agent    string
	Listener string
	Os       int
	Groups   []CommandGroup
}

////////////////////

type CommandRegistry struct {
	mu      sync.RWMutex
	groups  map[string]map[string]map[int][]CommandGroup // agentName → listener → os → []CommandGroup
	engines map[string]*ScriptEngine                     // agentName → engine
}

func NewCommandRegistry() *CommandRegistry {
	return &CommandRegistry{
		groups:  make(map[string]map[string]map[int][]CommandGroup),
		engines: make(map[string]*ScriptEngine),
	}
}

func (r *CommandRegistry) RegisterGroups(agentName string, listener string, os int, groups []CommandGroup, engine *ScriptEngine) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, ok := r.groups[agentName]; !ok {
		r.groups[agentName] = make(map[string]map[int][]CommandGroup)
	}
	if _, ok := r.groups[agentName][listener]; !ok {
		r.groups[agentName][listener] = make(map[int][]CommandGroup)
	}
	for i := range groups {
		groups[i].Engine = engine
	}
	r.groups[agentName][listener][os] = append(r.groups[agentName][listener][os], groups...)
	r.engines[agentName] = engine
}

// /---
func (r *CommandRegistry) UnregisterAgent(agentName string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	delete(r.groups, agentName)
	delete(r.engines, agentName)
}

// /---
func (r *CommandRegistry) UnregisterByScriptName(scriptName string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	for agentName, listenerMap := range r.groups {
		for listener, osMap := range listenerMap {
			for osType, groups := range osMap {
				var filtered []CommandGroup
				for _, g := range groups {
					if g.ScriptName != scriptName {
						filtered = append(filtered, g)
					}
				}
				if len(filtered) == 0 {
					delete(osMap, osType)
				} else {
					r.groups[agentName][listener][osType] = filtered
				}
			}
			if len(osMap) == 0 {
				delete(listenerMap, listener)
			}
		}
		if len(listenerMap) == 0 {
			delete(r.groups, agentName)
			delete(r.engines, agentName)
		}
	}
}

func (r *CommandRegistry) resolveInGroups(groups []CommandGroup, commandName string) *ResolvedCommand {
	for i := range groups {
		for j := range groups[i].Commands {
			if groups[i].Commands[j].Name == commandName {
				return &ResolvedCommand{
					Group:   &groups[i],
					Command: &groups[i].Commands[j],
					Engine:  groups[i].Engine,
				}
			}
		}
	}
	return nil
}

func (r *CommandRegistry) Resolve(agentName string, listener string, os int, commandName string) *ResolvedCommand {
	r.mu.RLock()
	defer r.mu.RUnlock()

	listenerMap, ok := r.groups[agentName]
	if !ok {
		return nil
	}

	if listener != "" {
		if osMap, ok := listenerMap[listener]; ok {
			if groups, ok := osMap[os]; ok {
				if res := r.resolveInGroups(groups, commandName); res != nil {
					return res
				}
			}
		}
	}

	if osMap, ok := listenerMap[""]; ok {
		if groups, ok := osMap[os]; ok {
			return r.resolveInGroups(groups, commandName)
		}
	}

	return nil
}

func (r *CommandRegistry) resolveSubInGroups(groups []CommandGroup, commandName string, subcommandName string) *ResolvedCommand {
	for i := range groups {
		for j := range groups[i].Commands {
			if groups[i].Commands[j].Name == commandName {
				for k := range groups[i].Commands[j].Subcommands {
					if groups[i].Commands[j].Subcommands[k].Name == subcommandName {
						return &ResolvedCommand{
							Group:      &groups[i],
							Command:    &groups[i].Commands[j],
							Subcommand: &groups[i].Commands[j].Subcommands[k],
							Engine:     groups[i].Engine,
						}
					}
				}
				return nil
			}
		}
	}
	return nil
}

func (r *CommandRegistry) ResolveSubcommand(agentName string, listener string, os int, commandName string, subcommandName string) *ResolvedCommand {
	r.mu.RLock()
	defer r.mu.RUnlock()

	listenerMap, ok := r.groups[agentName]
	if !ok {
		return nil
	}

	if listener != "" {
		if osMap, ok := listenerMap[listener]; ok {
			if groups, ok := osMap[os]; ok {
				if res := r.resolveSubInGroups(groups, commandName, subcommandName); res != nil {
					return res
				}
			}
		}
	}

	if osMap, ok := listenerMap[""]; ok {
		if groups, ok := osMap[os]; ok {
			return r.resolveSubInGroups(groups, commandName, subcommandName)
		}
	}

	return nil
}

// /---
func (r *CommandRegistry) GetCommandsForAgent(agentName string, listener string, os int) []CommandGroup {
	r.mu.RLock()
	defer r.mu.RUnlock()

	listenerMap, ok := r.groups[agentName]
	if !ok {
		return nil
	}

	var result []CommandGroup

	if listener != "" {
		if osMap, ok := listenerMap[listener]; ok {
			if groups, ok := osMap[os]; ok {
				result = append(result, groups...)
			}
		}
	}

	if osMap, ok := listenerMap[""]; ok {
		if groups, ok := osMap[os]; ok {
			result = append(result, groups...)
		}
	}

	return result
}

func (r *CommandRegistry) GetAllCommands() map[string]map[string]map[int][]CommandGroup {
	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make(map[string]map[string]map[int][]CommandGroup)
	for agent, listenerMap := range r.groups {
		result[agent] = make(map[string]map[int][]CommandGroup)
		for listener, osMap := range listenerMap {
			result[agent][listener] = make(map[int][]CommandGroup)
			for os, groups := range osMap {
				groupsCopy := make([]CommandGroup, len(groups))
				copy(groupsCopy, groups)
				result[agent][listener][os] = groupsCopy
			}
		}
	}
	return result
}

// /---
func (r *CommandRegistry) HasAgent(agentName string) bool {
	r.mu.RLock()
	defer r.mu.RUnlock()
	_, ok := r.groups[agentName]
	return ok
}
