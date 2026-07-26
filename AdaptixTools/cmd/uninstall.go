package cmd

import (
	"github.com/spf13/cobra"

	"axtool/internal/install"
)

var uninstallClearSource bool

var uninstallCmd = &cobra.Command{
	Use:   "uninstall <name>",
	Short: "Remove an installed plugin",
	Long:  "Remove release, go.work entry, profile entry, and state. Source tree is kept unless -c/--clear-source.",
	Args:  cobra.ExactArgs(1),
	RunE:  runUninstall,
}

func init() {
	uninstallCmd.Flags().BoolVarP(&uninstallClearSource, "clear-source", "c", false, "also delete the plugin source tree under AdaptixServer/")
	extCmd.AddCommand(uninstallCmd)
}

func runUninstall(c *cobra.Command, args []string) error {
	projectRoot, serverDir, _, err := loadProject()
	if err != nil {
		return err
	}
	return install.Uninstall(install.UninstallOptions{
		ProjectRoot: projectRoot,
		ServerDir:   serverDir,
		ClearSource: uninstallClearSource,
		Out:         colorOut(c.OutOrStderr()),
	}, args[0])
}
