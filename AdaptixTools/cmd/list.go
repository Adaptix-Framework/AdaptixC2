package cmd

import (
	"fmt"
	"text/tabwriter"

	"github.com/spf13/cobra"

	"axtool/internal/state"
)

var listCmd = &cobra.Command{
	Use:     "list",
	Aliases: []string{"ls"},
	Short:   "List installed plugins",
	Args:    cobra.NoArgs,
	RunE: func(c *cobra.Command, _ []string) error {
		_, serverDir, srv, err := loadProject()
		if err != nil {
			return err
		}
		st, err := state.Load(serverDir)
		if err != nil {
			return err
		}
		out := colorOut(c.OutOrStdout())
		if len(st.Plugins) == 0 {
			fmt.Fprintln(out, "(no plugins installed)")
			return nil
		}
		w := tabwriter.NewWriter(out, 0, 0, 2, ' ', 0)
		fmt.Fprintln(w, "NAME\tVERSION\tTYPE\tSOURCE\tINSTALLED_AT")
		for _, p := range st.Plugins {
			fmt.Fprintf(w, "%s\t%s\t%s\t%s\t%s\n", p.Name, p.Version, p.Type, p.Source, p.InstalledAt)
		}
		_ = w.Flush()
		if srv.ServerVersion != "" {
			fmt.Fprintf(out, "\nserver: %s\n", srv.ServerVersion)
		}
		return nil
	},
}

func init() {
	extCmd.AddCommand(listCmd)
}
