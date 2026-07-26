package cmd

import (
	"fmt"
	"strings"

	"github.com/spf13/cobra"

	"axtool/internal/profile"
)

var profileCmd = &cobra.Command{
	Use:   "profile",
	Short: "Runtime profile.yaml (Teamserver host/port/...)",
}

var profileShowCmd = &cobra.Command{
	Use:   "show",
	Short: "Show Teamserver scalar fields from the runtime profile",
	Args:  cobra.NoArgs,
	RunE:  runProfileShow,
}

var profileGetCmd = &cobra.Command{
	Use:   "get <key>",
	Short: "Get one Teamserver field (interface, port, endpoint, …)",
	Args:  cobra.ExactArgs(1),
	RunE:  runProfileGet,
}

var profileSetCmd = &cobra.Command{
	Use:   "set <key=value> [key=value...]",
	Short: "Set one or more Teamserver fields",
	Args:  cobra.MinimumNArgs(1),
	RunE:  runProfileSet,
}

func init() {
	profileCmd.AddCommand(profileShowCmd, profileGetCmd, profileSetCmd)
	rootCmd.AddCommand(profileCmd)
}

func openRuntimeProfile() (*profile.Patcher, string, error) {
	root, _, srv, err := loadProject()
	if err != nil {
		return nil, "", err
	}
	path := srv.ProfilePath(root)
	p, err := profile.New(path)
	if err != nil {
		return nil, "", err
	}
	return p, path, nil
}

func runProfileShow(c *cobra.Command, _ []string) error {
	p, path, err := openRuntimeProfile()
	if err != nil {
		return err
	}
	out := colorOut(c.OutOrStdout())
	fmt.Fprintf(out, "profile: %s\n", path)
	pairs := p.ListTeamserverScalars()
	if len(pairs) == 0 {
		fmt.Fprintln(out, "(no known Teamserver scalars found)")
		return nil
	}
	for _, kv := range pairs {
		fmt.Fprintf(out, "  %s: %s\n", kv[0], kv[1])
	}
	return nil
}

func runProfileGet(c *cobra.Command, args []string) error {
	key := strings.TrimSpace(args[0])
	p, _, err := openRuntimeProfile()
	if err != nil {
		return err
	}
	v, ok := p.GetTeamserver(key)
	if !ok {
		return fmt.Errorf("key %q not found under Teamserver: (known: %s)", key, strings.Join(profile.TeamserverScalars, ", "))
	}
	fmt.Fprintln(colorOut(c.OutOrStdout()), v)
	return nil
}

func runProfileSet(c *cobra.Command, args []string) error {
	p, path, err := openRuntimeProfile()
	if err != nil {
		return err
	}
	out := colorOut(c.OutOrStderr())
	for _, a := range args {
		k, v, ok := strings.Cut(a, "=")
		if !ok || strings.TrimSpace(k) == "" {
			return fmt.Errorf("expected key=value, got %q", a)
		}
		k = strings.TrimSpace(k)
		v = strings.TrimSpace(v)
		if err := p.SetTeamserver(k, v); err != nil {
			return fmt.Errorf("%s: %w", k, err)
		}
		fmt.Fprintf(out, "[ok] %s = %s\n", k, v)
	}
	if err := p.Save(); err != nil {
		return err
	}
	fmt.Fprintf(out, "[ok] wrote %s\n", path)
	return nil
}
