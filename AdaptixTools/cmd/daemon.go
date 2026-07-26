package cmd

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/cobra"

	"axtool/internal/project"
	"axtool/internal/service"
)

var (
	daemonUnitName  string
	daemonUserMode  bool
	daemonRunAs     string
	daemonGroup     string
	daemonDebug     bool
	daemonNoEnable  bool
	daemonNoDisable bool
	daemonStart     bool
	daemonNoStop    bool
	daemonFollow    bool
	daemonLines     int
)

var daemonCmd = &cobra.Command{
	Use:   "daemon",
	Short: "Systemd unit for teamserver",
}

var daemonInstallCmd = &cobra.Command{
	Use:   "install",
	Short: "Install systemd unit and enable it (default)",
	Args:  cobra.NoArgs,
	RunE:  runDaemonInstall,
}

var daemonUninstallCmd = &cobra.Command{
	Use:   "uninstall",
	Short: "Stop, disable (default), and remove the unit",
	Args:  cobra.NoArgs,
	RunE:  runDaemonUninstall,
}

var daemonStartCmd = &cobra.Command{
	Use:   "start",
	Short: "Start the teamserver daemon",
	Args:  cobra.NoArgs,
	RunE: func(c *cobra.Command, _ []string) error {
		return runDaemonAction(c, "start")
	},
}

var daemonStopCmd = &cobra.Command{
	Use:   "stop",
	Short: "Stop the teamserver daemon",
	Args:  cobra.NoArgs,
	RunE: func(c *cobra.Command, _ []string) error {
		return runDaemonAction(c, "stop")
	},
}

var daemonRestartCmd = &cobra.Command{
	Use:   "restart",
	Short: "Restart the teamserver daemon",
	Args:  cobra.NoArgs,
	RunE: func(c *cobra.Command, _ []string) error {
		return runDaemonAction(c, "restart")
	},
}

var daemonStatusCmd = &cobra.Command{
	Use:   "status",
	Short: "Show systemd status",
	Args:  cobra.NoArgs,
	RunE: func(c *cobra.Command, _ []string) error {
		return runDaemonAction(c, "status")
	},
}

var daemonLogsCmd = &cobra.Command{
	Use:   "logs",
	Short: "Show journalctl logs for the unit",
	Args:  cobra.NoArgs,
	RunE:  runDaemonLogs,
}

func init() {
	for _, c := range []*cobra.Command{
		daemonInstallCmd, daemonUninstallCmd,
		daemonStartCmd, daemonStopCmd, daemonRestartCmd,
		daemonStatusCmd, daemonLogsCmd,
	} {
		c.Flags().StringVar(&daemonUnitName, "name", "", "unit name override (default: systemd.name in adaptix.spec, else adaptixserver)")
		c.Flags().BoolVar(&daemonUserMode, "user", false, "force systemd --user (or set systemd.user_mode in adaptix.spec)")
	}

	daemonInstallCmd.Flags().StringVar(&daemonRunAs, "run-as", "", "User= override (default: systemd.user in adaptix.spec)")
	daemonInstallCmd.Flags().StringVar(&daemonGroup, "group", "", "Group= override (default: systemd.group in adaptix.spec)")
	daemonInstallCmd.Flags().BoolVar(&daemonDebug, "debug", false, "pass -debug (or systemd.debug: true in adaptix.spec)")
	daemonInstallCmd.Flags().BoolVar(&daemonNoEnable, "no-enable", false, "do not systemctl enable after install")
	daemonInstallCmd.Flags().BoolVar(&daemonStart, "start", false, "systemctl start after install")

	daemonUninstallCmd.Flags().BoolVar(&daemonNoDisable, "no-disable", false, "do not systemctl disable before remove")
	daemonUninstallCmd.Flags().BoolVar(&daemonNoStop, "no-stop", false, "do not stop the unit before remove")

	daemonLogsCmd.Flags().BoolVarP(&daemonFollow, "follow", "f", false, "follow log output")
	daemonLogsCmd.Flags().IntVarP(&daemonLines, "lines", "n", 100, "number of log lines")

	daemonCmd.AddCommand(
		daemonInstallCmd, daemonUninstallCmd,
		daemonStartCmd, daemonStopCmd, daemonRestartCmd,
		daemonStatusCmd, daemonLogsCmd,
	)

}

func daemonOptsFromLayout() (service.Options, project.Layout, error) {
	layout, err := resolveLayout()
	if err != nil {
		return service.Options{}, layout, err
	}
	ss := layout.Spec.Systemd

	unitName := service.DefaultUnitName
	if ss.Name != "" {
		unitName = ss.Name
	}
	if daemonUnitName != "" {
		unitName = daemonUnitName
	}

	userMode := ss.UserMode || daemonUserMode
	debug := ss.Debug || daemonDebug

	runAs := ss.User
	if daemonRunAs != "" {
		runAs = daemonRunAs
	}
	if runAs == "root" {
		runAs = ""
	}
	group := ss.Group
	if daemonGroup != "" {
		group = daemonGroup
	}
	if group == "root" {
		group = ""
	}

	bin := filepath.Join(layout.DistDir, "adaptixserver")
	profileAbs := layout.Spec.ProfilePath(layout.ProjectRoot)
	profileArg := profileAbs
	if rel, err := filepath.Rel(layout.DistDir, profileAbs); err == nil && rel != "" && !filepath.IsAbs(rel) && rel != ".." && !hasDotDot(rel) {
		profileArg = rel
	}

	return service.Options{
		UnitName:   unitName,
		Binary:     bin,
		WorkingDir: layout.DistDir,
		Profile:    profileArg,
		User:       runAs,
		Group:      group,
		Debug:      debug,
		UserMode:   userMode,
	}, layout, nil
}

func hasDotDot(rel string) bool {
	for _, seg := range strings.Split(filepath.ToSlash(rel), "/") {
		if seg == ".." {
			return true
		}
	}
	return false
}

func runDaemonInstall(c *cobra.Command, _ []string) error {
	out := colorOut(c.OutOrStderr())
	if err := service.WhichSystemctl(); err != nil {
		return err
	}
	opts, layout, err := daemonOptsFromLayout()
	if err != nil {
		return err
	}
	if _, err := os.Stat(opts.Binary); err != nil {
		return fmt.Errorf("binary not found: %s (run: axtool build server)", opts.Binary)
	}

	prof := layout.Spec.ProfilePath(layout.ProjectRoot)
	if _, err := os.Stat(prof); err != nil {
		fmt.Fprintf(out, "[warn] profile not found: %s\n", prof)
	}

	path, err := service.Install(opts)
	if err != nil {
		return err
	}
	fmt.Fprintf(out, "[ok] wrote %s\n", path)

	if err := service.DaemonReload(opts.UserMode); err != nil {
		return fmt.Errorf("daemon-reload: %w", err)
	}
	if !daemonNoEnable {
		if err := service.Enable(opts.ResolvedUnitName(), opts.UserMode); err != nil {
			return fmt.Errorf("enable: %w", err)
		}
		fmt.Fprintf(out, "[ok] enabled %s\n", opts.UnitFileName())
	}
	if daemonStart {
		if err := service.Start(opts.ResolvedUnitName(), opts.UserMode); err != nil {
			return fmt.Errorf("start: %w", err)
		}
		fmt.Fprintf(out, "[ok] started %s\n", opts.UnitFileName())
	} else {
		fmt.Fprintf(out, "[hint] start with: axtool <spec> server daemon start\n")
	}
	return nil
}

func runDaemonUninstall(c *cobra.Command, _ []string) error {
	out := colorOut(c.OutOrStderr())
	if err := service.WhichSystemctl(); err != nil {
		return err
	}
	opts, _, err := daemonOptsFromLayout()
	if err != nil {
		return err
	}
	name := opts.ResolvedUnitName()

	if !daemonNoStop {
		_ = service.Stop(name, opts.UserMode)
	}
	if !daemonNoDisable {
		if err := service.Disable(name, opts.UserMode); err != nil {

			fmt.Fprintf(out, "[warn] disable: %v\n", err)
		} else {
			fmt.Fprintf(out, "[ok] disabled %s\n", name+".service")
		}
	}
	path, err := service.RemoveUnit(opts)
	if err != nil {
		return err
	}
	fmt.Fprintf(out, "[ok] removed %s\n", path)
	if err := service.DaemonReload(opts.UserMode); err != nil {
		return fmt.Errorf("daemon-reload: %w", err)
	}
	_ = path
	return nil
}

func runDaemonAction(c *cobra.Command, action string) error {
	out := colorOut(c.OutOrStderr())
	if err := service.WhichSystemctl(); err != nil {
		return err
	}
	opts, _, err := daemonOptsFromLayout()
	if err != nil {
		return err
	}
	name := opts.ResolvedUnitName()
	switch action {
	case "start":
		err = service.Start(name, opts.UserMode)
	case "stop":
		err = service.Stop(name, opts.UserMode)
	case "restart":
		err = service.Restart(name, opts.UserMode)
	case "status":
		err = service.Status(name, opts.UserMode)
	default:
		return fmt.Errorf("unknown action %q", action)
	}
	if action != "status" && err == nil {
		fmt.Fprintf(out, "[ok] %s %s\n", action, name+".service")
	}
	return err
}

func runDaemonLogs(c *cobra.Command, _ []string) error {
	if err := service.WhichSystemctl(); err != nil {
		return err
	}
	opts, _, err := daemonOptsFromLayout()
	if err != nil {
		return err
	}
	name := opts.ResolvedUnitName()
	return service.Logs(name, opts.UserMode, daemonFollow, daemonLines)
}
