package cmd

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/spf13/cobra"

	"axtool/internal/spec"
)

var projectSpecPath string

var (
	projectOnce sync.Once
	projectRoot string
	serverDir   string
	projectSpec spec.ServerSpec
	projectErr  error
)

var commandsWithoutSpec = map[string]bool{
	"completion": true,
	"template":   true,
}

var commandsWithSpec = map[string]bool{
	"server":  true,
	"client":  true,
	"ext":     true,
	"profile": true,
}

var rootCmd = &cobra.Command{
	Use:          "axtool",
	Short:        "AdaptixC2 operator toolkit (adaptix.spec)",
	SilenceUsage: true,
}

const usageTemplate = `Usage:{{if eq .Name "axtool"}}
  axtool <adaptix.spec> <command> [flags]
  axtool template … | completion …{{else}}{{if eq (index .Annotations "needs_spec") "1"}}
  axtool <adaptix.spec> {{.CommandPath | trimAxtool}}{{if .HasAvailableLocalFlags}} [flags]{{end}}{{else}}
  {{.UseLine}}{{end}}{{end}}{{if .HasAvailableSubCommands}}

Commands:{{range .Commands}}{{if .IsAvailableCommand}}
  {{rpad .Name .NamePadding }} {{.Short}}{{end}}{{end}}{{end}}{{if .HasAvailableLocalFlags}}

Flags:
{{.LocalFlags.FlagUsages | trimTrailingWhitespaces}}{{end}}{{if .HasAvailableInheritedFlags}}

Global Flags:
{{.InheritedFlags.FlagUsages | trimTrailingWhitespaces}}{{end}}
`

func Execute() error {
	for _, c := range rootCmd.Commands() {
		if commandsWithSpec[c.Name()] {
			markNeedsSpec(c)
		}
	}
	os.Args = preprocessArgs(os.Args)
	return rootCmd.Execute()
}

func init() {
	cobra.AddTemplateFunc("trimAxtool", func(s string) string {
		s = strings.TrimSpace(s)
		s = strings.TrimPrefix(s, "axtool")
		return strings.TrimSpace(s)
	})
	rootCmd.SetUsageTemplate(usageTemplate)

	rootCmd.SetHelpCommand(&cobra.Command{Hidden: true})
}

func markNeedsSpec(c *cobra.Command) {
	if c.Annotations == nil {
		c.Annotations = map[string]string{}
	}
	c.Annotations["needs_spec"] = "1"
	for _, sub := range c.Commands() {
		markNeedsSpec(sub)
	}
}

func preprocessArgs(args []string) []string {
	if len(args) < 3 {
		return args
	}
	first := args[1]
	if strings.HasPrefix(first, "-") {
		return args
	}
	if commandsWithoutSpec[first] || commandsWithSpec[first] {
		return args
	}

	projectSpecPath = first
	return append([]string{args[0]}, args[2:]...)
}

func loadProject() (root, srvDir string, srv spec.ServerSpec, err error) {
	projectOnce.Do(func() {
		if projectSpecPath == "" {
			projectErr = errors.New("adaptix.spec is required: axtool <adaptix.spec> <command>")
			return
		}
		s, rootDir, e := spec.LoadProject(projectSpecPath)
		if e != nil {
			projectErr = e
			return
		}
		projectRoot = rootDir
		projectSpec = s
		serverDir = s.ResolvedServerDir(rootDir)
		if _, e := os.Stat(serverDir); e != nil {
			projectErr = fmt.Errorf("server_dir %s: %w", serverDir, e)
			return
		}
	})
	return projectRoot, serverDir, projectSpec, projectErr
}

func resolveProjectRoot() (string, error) {
	root, _, _, err := loadProject()
	return root, err
}

func resolveServerDir() (string, error) {
	_, sd, _, err := loadProject()
	return sd, err
}

func resolveProjectSpec() (spec.ServerSpec, error) {
	_, _, s, err := loadProject()
	return s, err
}

func absCLIPath(p string) (string, error) {
	if p == "" {
		return "", nil
	}
	return filepath.Abs(p)
}
