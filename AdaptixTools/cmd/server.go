package cmd

import (
	"fmt"
	"io"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"

	"axtool/internal/profile"
	"axtool/internal/project"
)

var (
	serverBuildGenCert     bool
	serverBuildForceCert   bool
	serverBuildInstallDeps bool
	serverBuildNoPackages  bool
	serverBuildNoProfile   bool
)

var serverCmd = &cobra.Command{
	Use:   "server",
	Short: "Teamserver: build and systemd daemon",
}

var serverBuildCmd = &cobra.Command{
	Use:   "build",
	Short: "Build adaptixserver into dist_dir (then install packages: from adaptix.spec)",
	Args:  cobra.NoArgs,
	RunE:  runServerBuild,
}

func init() {
	serverBuildCmd.Flags().BoolVarP(&serverBuildInstallDeps, "install-deps", "d", false, "install host build deps via apt (Linux)")
	serverBuildCmd.Flags().BoolVar(&serverBuildGenCert, "gen-cert", false, "generate TLS certs in dist_dir (server.rsa.key/crt)")
	serverBuildCmd.Flags().BoolVar(&serverBuildForceCert, "force-cert", false, "overwrite existing server.rsa.{key,crt}")
	serverBuildCmd.Flags().BoolVar(&serverBuildNoPackages, "no-packages", false, "do not install adaptix.spec packages: after build")
	serverBuildCmd.Flags().BoolVar(&serverBuildNoProfile, "no-profile", false, "do not copy or modify profile (profile: seed + prune + package registration)")

	serverCmd.AddCommand(serverBuildCmd, daemonCmd)
	rootCmd.AddCommand(serverCmd)
}

func resolveLayout() (project.Layout, error) {
	root, err := resolveProjectRoot()
	if err != nil {
		return project.Layout{}, err
	}
	return project.Resolve(root)
}

func runServerBuild(c *cobra.Command, _ []string) error {
	out := colorOut(c.OutOrStderr())
	layout, err := resolveLayout()
	if err != nil {
		return err
	}
	if serverBuildInstallDeps {
		srv, err := resolveProjectSpec()
		if err != nil {
			return err
		}
		pkgs := srv.Deps.AptPackages(true, false)

		pkgs = append(pkgs, collectLocalPackageAptDeps(srv)...)
		if err := project.InstallAptDeps(c.Context(), out, pkgs); err != nil {
			return err
		}
	}
	if err := layout.BuildServer(c.Context(), out, project.BuildOptions{NoProfile: serverBuildNoProfile}); err != nil {
		return err
	}
	if serverBuildGenCert {
		if err := layout.GenerateCerts(c.Context(), out, project.CertOptions{Force: serverBuildForceCert}); err != nil {
			return err
		}
	}

	if !serverBuildNoPackages {

		if err := installSpecPackages(c, true, serverBuildNoProfile); err != nil {
			return err
		}
	} else {
		fmt.Fprintln(out, "[packages] skipped (--no-packages)")
	}

	if serverBuildNoProfile {
		fmt.Fprintln(out, "[profile] skipped (--no-profile)")
	} else if err := reconcileDistProfile(out, layout); err != nil {
		return err
	}
	return nil
}

func reconcileDistProfile(out io.Writer, layout project.Layout) error {
	srv, err := resolveProjectSpec()
	if err != nil {
		return err
	}
	profilePath := srv.ProfilePath(layout.ProjectRoot)
	if _, err := os.Stat(profilePath); err != nil {
		return nil
	}
	prof, err := profile.New(profilePath)
	if err != nil {
		return err
	}

	base := filepath.Dir(profilePath)
	removed, err := prof.PruneMissing(profile.ListExtenders, base)
	if err != nil {
		return err
	}
	if len(removed) == 0 {
		return nil
	}
	if err := prof.Save(); err != nil {
		return err
	}
	fmt.Fprintf(out, "[profile] pruned %d missing extender(s) from %s\n", len(removed), profilePath)
	for _, r := range removed {
		fmt.Fprintf(out, "  - %s\n", r)
	}
	return nil
}
