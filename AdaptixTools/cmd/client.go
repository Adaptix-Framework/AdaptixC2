package cmd

import (
	"runtime"

	"github.com/spf13/cobra"

	"axtool/internal/project"
)

var (
	clientBuildJobs        int
	clientBuildInstallDeps bool
)

var clientCmd = &cobra.Command{
	Use:   "client",
	Short: "GUI client build",
}

var clientBuildCmd = &cobra.Command{
	Use:   "build",
	Short: "Build AdaptixClient into dist_dir from adaptix.spec",
	Args:  cobra.NoArgs,
	RunE:  runClientBuild,
}

func init() {
	clientBuildCmd.Flags().BoolVarP(&clientBuildInstallDeps, "install-deps", "d", false, "install host build deps via apt (Linux)")
	clientBuildCmd.Flags().IntVarP(&clientBuildJobs, "jobs", "j", 0, "make -jN (default: nproc)")
	clientCmd.AddCommand(clientBuildCmd)
	rootCmd.AddCommand(clientCmd)
}

func runClientBuild(c *cobra.Command, _ []string) error {
	out := colorOut(c.OutOrStderr())
	layout, err := resolveLayout()
	if err != nil {
		return err
	}
	jobs := clientBuildJobs
	if jobs <= 0 {
		jobs = runtime.NumCPU()
	}
	if clientBuildInstallDeps {
		srv, err := resolveProjectSpec()
		if err != nil {
			return err
		}
		if err := project.InstallAptDeps(c.Context(), out, srv.Deps.AptPackages(false, true)); err != nil {
			return err
		}
	}
	return layout.BuildClient(c.Context(), out, project.BuildOptions{Jobs: jobs})
}
