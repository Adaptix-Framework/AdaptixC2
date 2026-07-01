package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
	"time"
)

var (
	flagHost      string
	flagPort      int
	flagEndpoint  string
	flagUsername  string
	flagPassword  string
	flagJSON      bool
	flagOnline    bool
	flagShellType string
	flagTimeout   string
	flagOutput    string
	flagRows      int
	flagCols      int
)

func main() {
	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	cmd := os.Args[1]
	args := os.Args[2:]

	switch cmd {
	case "config":
		runConfigCmd(args)

	case "agent":
		runAgentCmd(args)

	case "listener":
		runListenerCmd(args)

	case "download":
		runDownloadCmd(args)

	case "exec":
		runExecCmd(args)

	case "-h", "--help", "help":
		printUsage()

	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n", cmd)
		printUsage()
		os.Exit(1)
	}
}

func runConfigCmd(args []string) {
	if len(args) == 0 {
		printUsage()
		os.Exit(1)
	}
	switch args[0] {
	case "set":
		runConfigSet(args[1:])
	case "show":
		parseArgs("config show", args[1:], true)
		runConfigShow(flagJSON)
	default:
		fmt.Fprintf(os.Stderr, "unknown config command: %s\n", args[0])
		os.Exit(1)
	}
}

func runAgentCmd(args []string) {
	if len(args) == 0 {
		printUsage()
		os.Exit(1)
	}
	switch args[0] {
	case "list":
		fs := newFlagSet("agent list")
		addGlobalFlags(fs)
		fs.BoolVar(&flagJSON, "json", false, "JSON output")
		fs.BoolVar(&flagOnline, "online", false, "only show online agents")
		_ = fs.Parse(args[1:])
		mergeConfig()
		execWithClient(func(c *Client) error {
			return runAgentList(c, flagJSON, flagOnline)
		})
	case "info":
		extra := parseArgs("agent info", args[1:], true)
		if len(extra) == 0 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil agent info <id>")
			os.Exit(1)
		}
		execWithClient(func(c *Client) error {
			return runAgentInfo(c, extra[0], flagJSON)
		})
	case "remove":
		extra := parseArgs("agent remove", args[1:], true)
		if len(extra) == 0 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil agent remove <id>...")
			os.Exit(1)
		}
		execWithClient(func(c *Client) error {
			return runAgentRemove(c, extra)
		})
	case "exec":
		runExecCmd(args[1:])
	case "terminal":
		fs := newFlagSet("agent terminal")
		addGlobalFlags(fs)
		fs.StringVar(&flagShellType, "shell", "", "shell type: sh, bash, cmd, powershell (auto-detect by default)")
		fs.IntVar(&flagRows, "rows", 0, "terminal rows")
		fs.IntVar(&flagCols, "cols", 0, "terminal columns")
		flags, pos := extractFlags(fs, args[1:])
		if err := fs.Parse(flags); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
		mergeConfig()
		if len(pos) == 0 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil agent terminal <id> [--shell sh|bash|cmd|powershell]")
			os.Exit(1)
		}
		agentID := pos[0]
		execWithClient(func(c *Client) error {
			return runAgentTerminal(c, agentID, flagShellType, flagRows, flagCols)
		})
	default:
		fmt.Fprintf(os.Stderr, "unknown agent command: %s\n", args[0])
		os.Exit(1)
	}
}

func runListenerCmd(args []string) {
	if len(args) == 0 {
		printUsage()
		os.Exit(1)
	}
	switch args[0] {
	case "list":
		parseArgs("listener list", args[1:], true)
		execWithClient(func(c *Client) error {
			return runListenerList(c, flagJSON)
		})
	case "start":
		extra := parseArgs("listener start", args[1:], true)
		if len(extra) < 3 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil listener start <type> <name> <config>")
			os.Exit(1)
		}
		execWithClient(func(c *Client) error {
			return runListenerStart(c, extra[0], extra[1], extra[2])
		})
	case "stop":
		extra := parseArgs("listener stop", args[1:], true)
		if len(extra) < 2 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil listener stop <type> <name>")
			os.Exit(1)
		}
		execWithClient(func(c *Client) error {
			return runListenerStop(c, extra[0], extra[1])
		})
	default:
		fmt.Fprintf(os.Stderr, "unknown listener command: %s\n", args[0])
		os.Exit(1)
	}
}

func runDownloadCmd(args []string) {
	if len(args) == 0 {
		printUsage()
		os.Exit(1)
	}
	switch args[0] {
	case "list":
		parseArgs("download list", args[1:], true)
		execWithClient(func(c *Client) error {
			return runDownloadList(c, flagJSON)
		})
	case "get":
		fs := newFlagSet("download get")
		addGlobalFlags(fs)
		fs.StringVar(&flagOutput, "o", "", "output file path")
		_ = fs.Parse(args[1:])
		mergeConfig()
		if fs.NArg() == 0 {
			fmt.Fprintln(os.Stderr, "usage: adaptix-cil download get <file-id> [-o path]")
			os.Exit(1)
		}
		execWithClient(func(c *Client) error {
			return runDownloadGet(c, fs.Arg(0), flagOutput)
		})
	default:
		fmt.Fprintf(os.Stderr, "unknown download command: %s\n", args[0])
		os.Exit(1)
	}
}

func runExecCmd(args []string) {
	fs := newFlagSet("exec")
	addGlobalFlags(fs)
	fs.BoolVar(&flagJSON, "json", false, "JSON output")
	fs.StringVar(&flagTimeout, "timeout", "60s", "command timeout")
	_ = fs.Parse(args)
	mergeConfig()

	if fs.NArg() < 2 {
		printUsage()
		os.Exit(1)
	}

	agentID := fs.Arg(0)
	cmdline := fs.Arg(1)
	extraArgs := fs.Args()[2:]

	timeout, err := time.ParseDuration(flagTimeout)
	if err != nil {
		fmt.Fprintf(os.Stderr, "invalid timeout: %v\n", err)
		os.Exit(1)
	}

	execWithClient(func(c *Client) error {
		return runAgentExec(c, agentID, cmdline, extraArgs, timeout, flagJSON)
	})
}

func newFlagSet(name string) *flag.FlagSet {
	return flag.NewFlagSet(name, flag.ContinueOnError)
}

func addGlobalFlags(fs *flag.FlagSet) {
	fs.StringVar(&flagHost, "host", "", "server host")
	fs.IntVar(&flagPort, "port", 0, "server port")
	fs.StringVar(&flagEndpoint, "endpoint", "", "server endpoint")
	fs.StringVar(&flagUsername, "username", "", "operator username")
	fs.StringVar(&flagPassword, "password", "", "operator password")
}

func parseArgs(name string, args []string, addJSON bool) []string {
	fs := newFlagSet(name)
	addGlobalFlags(fs)
	if addJSON {
		fs.BoolVar(&flagJSON, "json", false, "JSON output")
	}

	flags, positionals := extractFlags(fs, args)
	err := fs.Parse(flags)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
	mergeConfig()
	return positionals
}

func extractFlags(fs *flag.FlagSet, args []string) (flags []string, positionals []string) {
	i := 0
	for i < len(args) {
		arg := args[i]
		if strings.HasPrefix(arg, "--") {
			name := strings.TrimPrefix(arg, "--")
			if eqIdx := strings.IndexByte(name, '='); eqIdx >= 0 {
				name = name[:eqIdx]
			}
			if fs.Lookup(name) != nil {
				flags = append(flags, arg)
				if eqIdx := strings.IndexByte(arg, '='); eqIdx < 0 {
					f := fs.Lookup(name)
					if f != nil {
						if _, isBool := f.Value.(interface{ IsBoolFlag() bool }); !isBool {
							i++
							if i < len(args) {
								flags = append(flags, args[i])
							}
						}
					}
				}
				i++
				continue
			}
		} else if strings.HasPrefix(arg, "-") && len(arg) > 1 {
			name := strings.TrimPrefix(arg, "-")
			if eqIdx := strings.IndexByte(name, '='); eqIdx >= 0 {
				name = name[:eqIdx]
			}
			if fs.Lookup(name) != nil {
				flags = append(flags, arg)
				if eqIdx := strings.IndexByte(arg, '='); eqIdx < 0 {
					f := fs.Lookup(name)
					if f != nil {
						if _, isBool := f.Value.(interface{ IsBoolFlag() bool }); !isBool {
							i++
							if i < len(args) {
								flags = append(flags, args[i])
							}
						}
					}
				}
				i++
				continue
			}
		}
		positionals = append(positionals, arg)
		i++
	}
	return
}

func mergeConfig() {
	if flagHost != "" || flagPort != 0 || flagEndpoint != "" || flagUsername != "" || flagPassword != "" {
		return
	}
	cfg, err := loadConfig()
	if err != nil {
		return
	}
	if flagHost == "" {
		flagHost = cfg.Host
	}
	if flagPort == 0 {
		flagPort = cfg.Port
	}
	if flagEndpoint == "" {
		flagEndpoint = cfg.Endpoint
	}
	if flagUsername == "" {
		flagUsername = cfg.Username
	}
	if flagPassword == "" {
		flagPassword = cfg.Password
	}
}

func getEffectiveConfig() *Config {
	mergeConfig()
	if flagHost == "" {
		fmt.Fprintln(os.Stderr, "error: no host configured. Run 'adaptix-cil config set' or use --host")
		os.Exit(1)
	}
	return &Config{
		Host:     flagHost,
		Port:     flagPort,
		Endpoint: flagEndpoint,
		Username: flagUsername,
		Password: flagPassword,
	}
}

func execWithClient(fn func(*Client) error) {
	cfg := getEffectiveConfig()
	c := newClient(cfg)
	if err := c.login(); err != nil {
		fmt.Fprintf(os.Stderr, "error: login failed: %v\n", err)
		os.Exit(1)
	}
	if err := fn(c); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		c.disconnect()
		os.Exit(1)
	}
	c.disconnect()
}

func printUsage() {
	fmt.Println(`AdaptixCIL - CLI tool for AdaptixC2 server

USAGE:
  adaptix-cil config set   [--host HOST] [--port PORT] [--endpoint EP] [--username U] [--password P]
  adaptix-cil config show  [--json]

  adaptix-cil agent list     [--online] [--json]
  adaptix-cil agent info     <id> [--json]
  adaptix-cil agent exec     <id> <command> [--timeout DUR]
  adaptix-cil agent terminal <id> [--shell sh|bash|cmd|powershell] [--rows N] [--cols N]
  adaptix-cil agent remove   <id>...

  adaptix-cil listener list   [--json]
  adaptix-cil listener start  <type> <name> <config>
  adaptix-cil listener stop   <type> <name>

  adaptix-cil download list   [--json]
  adaptix-cil download get    <file-id> [-o PATH]

GLOBAL OPTIONS:
  --host HOST       Server host (overrides config)
  --port PORT       Server port (overrides config)
  --endpoint EP     Server endpoint (overrides config)
  --username U      Operator username (overrides config)
  --password P      Operator password (overrides config)
  --json            Output in JSON format

EXAMPLES:
  adaptix-cil config set --host 10.0.0.1 --port 4321 --username admin --password pass
  adaptix-cil agent list --online --json
  adaptix-cil agent exec abc123 "shell whoami"
  adaptix-cil agent terminal abc123
  adaptix-cil agent terminal abc123 --shell powershell

  adaptix-cil download list
  adaptix-cil download get 1a2b3c -o /tmp/out.bin`)
}


