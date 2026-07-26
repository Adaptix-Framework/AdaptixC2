package cmd

import (
	"github.com/spf13/cobra"
)

var extCmd = &cobra.Command{
	Use:   "ext",
	Short: "Extenders and AxScript kits",
}

func init() {
	rootCmd.AddCommand(extCmd)
}
