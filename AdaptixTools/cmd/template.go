package cmd

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"

	"axtool/internal/scaffold"
	"axtool/internal/spec"
)

var (
	templateFrom     string
	templateProtocol string
)

var templateCmd = &cobra.Command{
	Use:   "template <type> [name]",
	Short: "Scaffold from templates-extender (agent|listener|service|axscript)",
	Args:  cobra.RangeArgs(1, 2),
	RunE:  runTemplate,
}

func init() {
	templateCmd.Flags().StringVar(&templateFrom, "from", "", "templates root: local path or git source (default: github.com/Adaptix-Framework/templates-extender@main)")
	templateCmd.Flags().StringVar(&templateProtocol, "protocol", "custom", "listener protocol placeholder (_PROTOCOL_)")
	rootCmd.AddCommand(templateCmd)
}

func runTemplate(c *cobra.Command, args []string) error {
	typ := scaffold.Type(args[0])
	switch typ {
	case scaffold.TypeListener, scaffold.TypeAgent, scaffold.TypeService, scaffold.TypeAxScript:
	default:
		return fmt.Errorf("type must be listener|agent|service|axscript, got %q", args[0])
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

	if len(args) == 2 {
		if err := os.MkdirAll(target, 0o755); err != nil {
			return err
		}
	}

	out := colorOut(c.OutOrStdout())
	src := templateFrom
	if src == "" && typ != scaffold.TypeAxScript {
		fmt.Fprintf(out, "[template] fetching %s@%s …\n", scaffold.DefaultSource, scaffold.DefaultRef)
	}

	err = scaffold.Run(c.Context(), scaffold.Options{
		Type:      typ,
		Name:      name,
		TargetDir: target,
		Source:    src,
		Protocol:  templateProtocol,
	})
	if err != nil {
		return err
	}

	switch typ {
	case scaffold.TypeAxScript:
		fmt.Fprintf(out, "[ok] scaffolded axscript kit %q in %s\n", name, target)
		fmt.Fprintf(out, "  · next: axtool <adaptix.spec> ext install %s\n", target)
	default:
		fmt.Fprintf(out, "[ok] scaffolded %s %q in %s\n", typ, name, target)
		fmt.Fprintf(out, "  · template: Adaptix-Framework/templates-extender (%s_template)\n", typ)
		fmt.Fprintf(out, "  · next: axtool <adaptix.spec> ext install %s\n", target)
		if typ == scaffold.TypeAgent {
			fmt.Fprintf(out, "  · note: set supported listeners in the extender config (listeners:)\n")
		}
	}
	return nil
}
