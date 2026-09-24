package cmd

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"

	"axtool/internal/spec"
)

var initCmd = &cobra.Command{
	Use:   "init <type> [name]",
	Short: "Scaffold a new plugin with an axtool.spec",
	Long: `Scaffold a new plugin directory with a starter axtool.spec and source layout.

type must be one of: listener, agent, service.

If name is omitted, the current directory's base name is used. The command
refuses to overwrite an existing axtool.spec.`,
	Args: cobra.RangeArgs(1, 2),
	RunE: runInit,
}

func init() {
	rootCmd.AddCommand(initCmd)
}

func runInit(c *cobra.Command, args []string) error {
	typ := args[0]
	switch typ {
	case "listener", "agent", "service":
	default:
		return fmt.Errorf("type must be listener|agent|service, got %q", typ)
	}

	name := ""
	if len(args) == 2 {
		name = args[1]
	}
	cwd, err := os.Getwd()
	if err != nil {
		return err
	}
	if name == "" {
		name = filepath.Base(cwd)
	}
	if !spec.IsSafeName(name) {
		return fmt.Errorf("invalid plugin name %q: must match [a-z0-9][a-z0-9_-]*", name)
	}

	target := cwd
	if len(args) == 2 {
		target = filepath.Join(cwd, name)
	}
	specPath := filepath.Join(target, spec.PluginFileName)
	if _, err := os.Stat(specPath); err == nil {
		return errors.New("axtool.spec already exists in " + target)
	}
	if target != cwd {
		if err := os.MkdirAll(target, 0o755); err != nil {
			return err
		}
	}

	soName := name + ".so"
	switch typ {
	case "agent":
		soName = "agent_" + name + ".so"
	case "listener":
		soName = "listener_" + name + ".so"
	}

	specBody := fmt.Sprintf(`extenders:
  - name: %s
    version: 0.1.0
    type: %s
    description: "TODO: describe this plugin"
    author: TODO

    min_server_version: "v2.0"

    build:
      - make

    release:
      dir: dist/
`, name, typ)

	makefileBody := fmt.Sprintf(`GO_LDFLAGS ?= -s -w
ifeq ($(shell uname -s),Linux)
ifneq ($(filter aarch64 arm64,$(shell uname -m)),)
ifeq ($(shell command -v ld.gold 2>/dev/null),)
GO_LDFLAGS += -extldflags=-fuse-ld=bfd
endif
endif
endif

all: clean
	@ mkdir -p dist
	@ cp config.yaml ax_config.axs ./dist/ 2>/dev/null || true
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="$(GO_LDFLAGS)" -o ./dist/%s .
	@ echo "    -> dist/%s"

clean:
	@ rm -rf dist
`, soName, soName)

	configBody := fmt.Sprintf(`extender_type: "%s"
extender_file: "%s"
ax_file: "ax_config.axs"
`, typ, soName)

	if err := os.WriteFile(specPath, []byte(specBody), 0o644); err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(target, "Makefile"), []byte(makefileBody), 0o644); err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(target, "config.yaml"), []byte(configBody), 0o644); err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(target, "ax_config.axs"), []byte("// AxScript UI definitions for "+name+"\n"), 0o644); err != nil {
		return err
	}
	goModBody := fmt.Sprintf("module %s\n\ngo 1.26\n\nrequire github.com/Adaptix-Framework/axc2/v2 v2.0.2\n", name)
	if err := os.WriteFile(filepath.Join(target, "go.mod"), []byte(goModBody), 0o644); err != nil {
		return err
	}

	out := c.OutOrStdout()
	fmt.Fprintf(out, "scaffolded %s plugin %q in %s\n", typ, name, target)
	fmt.Fprintf(out, "  axtool.spec\n  Makefile\n  config.yaml\n  ax_config.axs\n  go.mod\n")
	fmt.Fprintf(out, "\nNext: implement pl_main.go with func InitPlugin(...) and `axtool validate %s`.\n", target)
	return nil
}

func validName(s string) bool {
	return spec.IsSafeName(s)
}
