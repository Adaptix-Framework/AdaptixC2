package axscript

import (
	"fmt"
	"sync"
)

type CommandRegistry struct {
	mu sync.RWMutex

	groups map[string]map[string]map[int][]CommandGroup // agentName → listener → os → []CommandGroup
	engines map[string]*ScriptEngine // agentName → engine
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
	r.groups[agentName][listener][os] = append(r.groups[agentName][listener][os], groups...)
	r.engines[agentName] = engine
}

func (r *CommandRegistry) UnregisterAgent(agentName string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	delete(r.groups, agentName)
	delete(r.engines, agentName)
}

func (r *CommandRegistry) resolveInGroups(groups []CommandGroup, engine *ScriptEngine, commandName string) *ResolvedCommand {
	for i := range groups {
		for j := range groups[i].Commands {
			if groups[i].Commands[j].Name == commandName {
				return &ResolvedCommand{
					Group:   &groups[i],
					Command: &groups[i].Commands[j],
					Engine:  engine,
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

	engine := r.engines[agentName]

	if listener != "" {
		if osMap, ok := listenerMap[listener]; ok {
			if groups, ok := osMap[os]; ok {
				if res := r.resolveInGroups(groups, engine, commandName); res != nil {
					return res
				}
			}
		}
	}

	if osMap, ok := listenerMap[""]; ok {
		if groups, ok := osMap[os]; ok {
			return r.resolveInGroups(groups, engine, commandName)
		}
	}

	return nil
}

func (r *CommandRegistry) resolveSubInGroups(groups []CommandGroup, engine *ScriptEngine, commandName string, subcommandName string) *ResolvedCommand {
	for i := range groups {
		for j := range groups[i].Commands {
			if groups[i].Commands[j].Name == commandName {
				for k := range groups[i].Commands[j].Subcommands {
					if groups[i].Commands[j].Subcommands[k].Name == subcommandName {
						return &ResolvedCommand{
							Group:      &groups[i],
							Command:    &groups[i].Commands[j],
							Subcommand: &groups[i].Commands[j].Subcommands[k],
							Engine:     engine,
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

	engine := r.engines[agentName]

	if listener != "" {
		if osMap, ok := listenerMap[listener]; ok {
			if groups, ok := osMap[os]; ok {
				if res := r.resolveSubInGroups(groups, engine, commandName, subcommandName); res != nil {
					return res
				}
			}
		}
	}

	if osMap, ok := listenerMap[""]; ok {
		if groups, ok := osMap[os]; ok {
			return r.resolveSubInGroups(groups, engine, commandName, subcommandName)
		}
	}

	return nil
}

func (r *CommandRegistry) ResolveFromCmdline(agentName string, listener string, os int, cmdline string) (*ResolvedCommand, error) {
	tokens := Tokenize(cmdline)
	if len(tokens) == 0 {
		return nil, fmt.Errorf("empty command line")
	}

	commandName := tokens[0]
	resolved := r.Resolve(agentName, listener, os, commandName)
	if resolved == nil {
		return nil, fmt.Errorf("command '%s' not found for agent '%s' os=%d", commandName, agentName, os)
	}

	if len(resolved.Command.Subcommands) > 0 {
		if len(tokens) < 2 {
			return nil, fmt.Errorf("subcommand required for '%s'", commandName)
		}
		subName := tokens[1]
		resolved = r.ResolveSubcommand(agentName, listener, os, commandName, subName)
		if resolved == nil {
			return nil, fmt.Errorf("subcommand '%s' not found for command '%s'", subName, commandName)
		}
	}

	return resolved, nil
}

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

func (r *CommandRegistry) HasAgent(agentName string) bool {
	r.mu.RLock()
	defer r.mu.RUnlock()
	_, ok := r.groups[agentName]
	return ok
}
