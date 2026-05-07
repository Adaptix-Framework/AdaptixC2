package axscript

import (
	"fmt"
	"strconv"
	"strings"
)

func Tokenize(cmdline string) []string {
	var tokens []string
	var token strings.Builder
	inQuotes := false
	runes := []rune(cmdline)
	length := len(runes)

	for i := 0; i < length; {
		c := runes[i]

		if c == ' ' && !inQuotes {
			if token.Len() > 0 {
				tokens = append(tokens, token.String())
				token.Reset()
			}
			i++
			continue
		}

		if c == '"' {
			inQuotes = !inQuotes
			i++
			continue
		}

		if c == '\\' {
			numBS := 0
			for i < length && runes[i] == '\\' {
				numBS++
				i++
			}
			if i < length && runes[i] == '"' {
				for j := 0; j < numBS/2; j++ {
					token.WriteRune('\\')
				}
				if numBS%2 == 0 {
					inQuotes = !inQuotes
				} else {
					token.WriteRune('"')
				}
				i++
			} else {
				for j := 0; j < numBS; j++ {
					token.WriteRune('\\')
				}
			}
			continue
		}

		token.WriteRune(c)
		i++
	}

	if token.Len() > 0 {
		tokens = append(tokens, token.String())
	}

	return tokens
}

func ParseCommand(cmdline string, resolved *ResolvedCommand) (*ParsedCommand, error) {
	tokens := Tokenize(cmdline)
	if len(tokens) == 0 {
		return nil, fmt.Errorf("empty command line")
	}

	result := &ParsedCommand{
		Args: make(map[string]interface{}),
	}

	commandName := tokens[0]
	result.CommandName = commandName
	result.Args["command"] = commandName

	var cmdDef *CommandDef
	argTokens := tokens[1:]

	if resolved.Subcommand != nil {
		if len(tokens) < 2 {
			return nil, fmt.Errorf("subcommand required for '%s'", commandName)
		}
		result.SubcommandName = tokens[1]
		result.Args["subcommand"] = tokens[1]
		cmdDef = resolved.Subcommand
		argTokens = tokens[2:]
	} else {
		cmdDef = resolved.Command
	}

	parsedArgsMap := make(map[string]string)
	var lastKey string

	for i := 0; i < len(argTokens); i++ {
		arg := argTokens[i]
		isWideArgs := true

		for _, commandArg := range cmdDef.Args {
			if commandArg.Flag {
				if commandArg.Type == ArgTypeBool && commandArg.Mark == arg {
					parsedArgsMap[commandArg.Mark] = "true"
					lastKey = commandArg.Mark
					isWideArgs = false
					break
				} else if commandArg.Mark == arg && i+1 < len(argTokens) {
					i++
					parsedArgsMap[commandArg.Name] = argTokens[i]
					lastKey = commandArg.Name
					isWideArgs = false
					break
				}
			} else if _, exists := parsedArgsMap[commandArg.Name]; !exists {
				parsedArgsMap[commandArg.Name] = arg
				lastKey = commandArg.Name
				isWideArgs = false
				break
			}
		}

		if isWideArgs && lastKey != "" {
			var wide strings.Builder
			for j := i; j < len(argTokens); j++ {
				wide.WriteString(" ")
				wide.WriteString(argTokens[j])
			}
			parsedArgsMap[lastKey] += wide.String()
			break
		}
	}

	for _, commandArg := range cmdDef.Args {
		nameKey := commandArg.Name
		if commandArg.Type == ArgTypeBool {
			nameKey = commandArg.Mark
		}

		if val, exists := parsedArgsMap[nameKey]; exists {
			switch commandArg.Type {
			case ArgTypeString:
				result.Args[commandArg.Name] = val
			case ArgTypeInt:
				intVal, err := strconv.Atoi(strings.TrimSpace(val))
				if err != nil {
					return nil, fmt.Errorf("invalid integer value for argument '%s': %s", commandArg.Name, val)
				}
				result.Args[commandArg.Name] = intVal
			case ArgTypeBool:
				result.Args[commandArg.Mark] = val == "true"
			case ArgTypeFile:
				result.FileArgs = append(result.FileArgs, FileArgInfo{
					ArgName:      commandArg.Name,
					OriginalPath: val,
					Required:     commandArg.Required,
				})
				result.Args[commandArg.Name] = nil
			}
		} else if commandArg.Required {
			if commandArg.DefaultValue == nil && !commandArg.DefaultUsed {
				return nil, fmt.Errorf("missing required argument: %s", commandArg.Name)
			}
			switch commandArg.Type {
			case ArgTypeString:
				if s, ok := commandArg.DefaultValue.(string); ok {
					result.Args[commandArg.Name] = s
				} else {
					return nil, fmt.Errorf("missing required argument: %s", commandArg.Name)
				}
			case ArgTypeInt:
				switch v := commandArg.DefaultValue.(type) {
				case int:
					result.Args[commandArg.Name] = v
				case int64:
					result.Args[commandArg.Name] = int(v)
				case float64:
					result.Args[commandArg.Name] = int(v)
				default:
					return nil, fmt.Errorf("missing required argument: %s", commandArg.Name)
				}
			case ArgTypeBool:
				if b, ok := commandArg.DefaultValue.(bool); ok {
					result.Args[commandArg.Mark] = b
				} else {
					return nil, fmt.Errorf("missing required argument: %s", commandArg.Name)
				}
			}
		}
	}

	msg := cmdDef.Message
	if msg != "" {
		for k, v := range parsedArgsMap {
			param := "<" + k + ">"
			if strings.Contains(msg, param) {
				msg = strings.ReplaceAll(msg, param, v)
			}
		}
		result.Message = msg
		result.Args["message"] = msg
	}

	return result, nil
}
