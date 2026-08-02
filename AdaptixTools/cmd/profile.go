package cmd

import (
	"fmt"
	"io"
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
	Short: "Show all available Teamserver scalar fields and their values",
	Args:  cobra.NoArgs,
	RunE:  runProfileShow,
}

var profileGetCmd = &cobra.Command{
	Use:   "get <key|all>",
	Short: "Get one Teamserver field, or all available parameters (get all)",
	Args:  cobra.ExactArgs(1),
	RunE:  runProfileGet,
}

var profileSetCmd = &cobra.Command{
	Use:   "set <key=value> [key=value...]",
	Short: "Set one or more Teamserver scalar fields",
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

func printTeamserverScalars(out io.Writer, p *profile.Patcher, withPath string) {
	if withPath != "" {
		fmt.Fprintf(out, "profile: %s\n", withPath)
	}
	for _, kv := range p.ListTeamserverScalars() {
		fmt.Fprintf(out, "  %s: %s\n", kv[0], kv[1])
	}
}

func runProfileShow(c *cobra.Command, _ []string) error {
	p, path, err := openRuntimeProfile()
	if err != nil {
		return err
	}
	printTeamserverScalars(colorOut(c.OutOrStdout()), p, path)
	return nil
}

func runProfileGet(c *cobra.Command, args []string) error {
	key := strings.TrimSpace(args[0])
	p, path, err := openRuntimeProfile()
	if err != nil {
		return err
	}
	out := colorOut(c.OutOrStdout())
	if strings.EqualFold(key, "all") {
		printTeamserverScalars(out, p, path)
		return nil
	}
	if !profile.IsTeamserverScalar(key) {
		return fmt.Errorf("unknown key %q (available: %s)", key, strings.Join(profile.TeamserverScalars, ", "))
	}
	v, ok := p.GetTeamserver(key)
	if !ok {
		fmt.Fprintln(out, "")
		return nil
	}
	fmt.Fprintln(out, v)
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
		if !profile.IsTeamserverScalar(k) {
			return fmt.Errorf("unknown key %q (available: %s)", k, strings.Join(profile.TeamserverScalars, ", "))
		}
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
