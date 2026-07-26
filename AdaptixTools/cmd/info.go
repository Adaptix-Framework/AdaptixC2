package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"axtool/internal/state"
)

var infoCmd = &cobra.Command{
	Use:   "info <name>",
	Short: "Show details of an installed plugin",
	Args:  cobra.ExactArgs(1),
	RunE: func(c *cobra.Command, args []string) error {
		serverDir, err := resolveServerDir()
		if err != nil {
			return err
		}
		st, err := state.Load(serverDir)
		if err != nil {
			return err
		}
		entry := st.Find(args[0])
		if entry == nil {
			return fmt.Errorf("plugin %q is not installed", args[0])
		}
		out := colorOut(c.OutOrStdout())
		fmt.Fprintf(out, "name:         %s\n", entry.Name)
		fmt.Fprintf(out, "version:      %s\n", entry.Version)
		fmt.Fprintf(out, "type:         %s\n", entry.Type)
		fmt.Fprintf(out, "source:       %s\n", entry.Source)
		if entry.Commit != "" {
			fmt.Fprintf(out, "commit:       %s\n", entry.Commit)
		}
		fmt.Fprintf(out, "installed_at: %s\n", entry.InstalledAt)
		fmt.Fprintf(out, "source_path:  %s\n", entry.SourcePath)
		fmt.Fprintf(out, "release_path: %s\n", entry.ReleasePath)
		fmt.Fprintf(out, "profile:      %s\n", entry.ProfileEntry)
		return nil
	},
}

func init() {
	extCmd.AddCommand(infoCmd)
}
