package axscript

import (
	"github.com/dop251/goja"
)

const (
	ArgTypeString = "STRING"
	ArgTypeInt    = "INT"
	ArgTypeBool   = "BOOL"
	ArgTypeFile   = "FILE"
)

const (
	OsWindows = 1
	OsLinux   = 2
	OsMac     = 3
)

type ArgumentDef struct {
	Type         string      `json:"type"`
	Name         string      `json:"name"`
	Required     bool        `json:"required"`
	Flag         bool        `json:"flag"`
	Mark         string      `json:"mark"`
	Description  string      `json:"description"`
	DefaultUsed  bool        `json:"default_used"`
	DefaultValue interface{} `json:"default_value,omitempty"`
}

type CommandDef struct {
	Name        string        `json:"name"`
	Message     string        `json:"message"`
	Description string        `json:"description"`
	Example     string        `json:"example"`
	Args        []ArgumentDef `json:"args,omitempty"`
	Subcommands []CommandDef  `json:"subcommands,omitempty"`

	PreHookFunc  goja.Callable `json:"-"`
	HasPreHook   bool          `json:"has_pre_hook"`
	PostHookFunc goja.Callable `json:"-"`
	HasPostHook  bool          `json:"has_post_hook"`
	HandlerFunc  goja.Callable `json:"-"`
	HasHandler   bool          `json:"has_handler"`
}

type CommandGroup struct {
	GroupName  string       `json:"group_name"`
	ScriptName string       `json:"script_name"`
	Commands   []CommandDef `json:"commands"`
}

type RegisterCommandsResult struct {
	Windows *CommandGroup
	Linux   *CommandGroup
	Mac     *CommandGroup
}

type ResolvedCommand struct {
	Group      *CommandGroup
	Command    *CommandDef
	Subcommand *CommandDef
	Engine     *ScriptEngine
}

type ParsedCommand struct {
	CommandName    string                 `json:"command"`
	SubcommandName string                 `json:"subcommand,omitempty"`
	Args           map[string]interface{} `json:"args"`
	Message        string                 `json:"message,omitempty"`
	FileArgs       []FileArgInfo          `json:"file_args,omitempty"`
}

type FileArgInfo struct {
	ArgName      string `json:"arg_name"`
	OriginalPath string `json:"original_path"`
	Required     bool   `json:"required"`
}

type PendingHook struct {
	ID      string
	Engine  *ScriptEngine
	Func    goja.Callable
	AgentID string
	Client  string
}

type ScriptInfo struct {
	Name       string `json:"name"`
	ScriptType string `json:"type"`
	AgentName  string `json:"agent_name,omitempty"`
	Path       string `json:"path,omitempty"`
}

func OsFromString(s string) int {
	switch s {
	case "windows":
		return OsWindows
	case "linux":
		return OsLinux
	case "macos", "mac":
		return OsMac
	default:
		return 0
	}
}

func OsToString(os int) string {
	switch os {
	case OsWindows:
		return "windows"
	case OsLinux:
		return "linux"
	case OsMac:
		return "macos"
	default:
		return "unknown"
	}
}
