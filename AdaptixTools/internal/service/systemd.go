package service

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"text/template"
)

const DefaultUnitName = "adaptixserver"

type Options struct {
	UnitName   string
	Binary     string
	WorkingDir string
	Profile    string
	User       string
	Group      string
	Debug      bool
	UserMode   bool
}

func (o Options) ResolvedUnitName() string {
	if o.UnitName != "" {
		return o.UnitName
	}
	return DefaultUnitName
}

func (o Options) UnitFileName() string {
	return o.ResolvedUnitName() + ".service"
}

func (o Options) UnitPath() (string, error) {
	name := o.UnitFileName()
	if o.UserMode {
		home, err := os.UserHomeDir()
		if err != nil {
			return "", err
		}
		return filepath.Join(home, ".config", "systemd", "user", name), nil
	}
	return filepath.Join("/etc/systemd/system", name), nil
}

const unitTmpl = `[Unit]
Description=AdaptixC2 Teamserver
Documentation=https://github.com/Adaptix-Framework
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
{{if .User}}User={{.User}}
{{end}}{{if .Group}}Group={{.Group}}
{{end}}WorkingDirectory={{.WorkingDir}}
ExecStart={{.ExecStart}}
Restart=on-failure
RestartSec=3
LimitNOFILE=65535
# Keep stdout/stderr in the journal
StandardOutput=journal
StandardError=journal
SyslogIdentifier={{.UnitName}}

[Install]
WantedBy={{.WantedBy}}
`

func RenderUnit(o Options) (string, error) {
	if o.Binary == "" {
		return "", fmt.Errorf("binary path is required")
	}
	if o.WorkingDir == "" {
		return "", fmt.Errorf("working directory is required")
	}
	profile := o.Profile
	if profile == "" {
		profile = "profile.yaml"
	}
	execStart := fmt.Sprintf("%s -profile %s", shellQuote(o.Binary), shellQuote(profile))
	if o.Debug {
		execStart += " -debug"
	}
	wanted := "multi-user.target"
	if o.UserMode {
		wanted = "default.target"
	}
	data := map[string]string{
		"User":       o.User,
		"Group":      o.Group,
		"WorkingDir": o.WorkingDir,
		"ExecStart":  execStart,
		"UnitName":   o.ResolvedUnitName(),
		"WantedBy":   wanted,
	}
	t, err := template.New("unit").Parse(unitTmpl)
	if err != nil {
		return "", err
	}
	var buf bytes.Buffer
	if err := t.Execute(&buf, data); err != nil {
		return "", err
	}
	return buf.String(), nil
}

func Install(o Options) (unitPath string, err error) {
	body, err := RenderUnit(o)
	if err != nil {
		return "", err
	}
	path, err := o.UnitPath()
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return "", fmt.Errorf("create unit dir: %w", err)
	}
	if err := os.WriteFile(path, []byte(body), 0o644); err != nil {
		return "", fmt.Errorf("write unit %s: %w (need root for system units? try --user)", path, err)
	}
	return path, nil
}

func RemoveUnit(o Options) (string, error) {
	path, err := o.UnitPath()
	if err != nil {
		return "", err
	}
	if err := os.Remove(path); err != nil && !os.IsNotExist(err) {
		return path, fmt.Errorf("remove unit %s: %w", path, err)
	}
	return path, nil
}

func systemctl(userMode bool, args ...string) error {
	cmdArgs := make([]string, 0, len(args)+1)
	if userMode {
		cmdArgs = append(cmdArgs, "--user")
	}
	cmdArgs = append(cmdArgs, args...)
	cmd := exec.Command("systemctl", cmdArgs...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func systemctlOut(userMode bool, args ...string) (string, error) {
	cmdArgs := make([]string, 0, len(args)+1)
	if userMode {
		cmdArgs = append(cmdArgs, "--user")
	}
	cmdArgs = append(cmdArgs, args...)
	cmd := exec.Command("systemctl", cmdArgs...)
	out, err := cmd.CombinedOutput()
	return string(out), err
}

func DaemonReload(userMode bool) error {
	return systemctl(userMode, "daemon-reload")
}

func Enable(unitName string, userMode bool) error {
	return systemctl(userMode, "enable", unitName+".service")
}

func Disable(unitName string, userMode bool) error {
	return systemctl(userMode, "disable", unitName+".service")
}

func Start(unitName string, userMode bool) error {
	return systemctl(userMode, "start", unitName+".service")
}

func Stop(unitName string, userMode bool) error {
	return systemctl(userMode, "stop", unitName+".service")
}

func Restart(unitName string, userMode bool) error {
	return systemctl(userMode, "restart", unitName+".service")
}

func Status(unitName string, userMode bool) error {
	return systemctl(userMode, "status", unitName+".service", "--no-pager")
}

func IsActive(unitName string, userMode bool) bool {
	out, err := systemctlOut(userMode, "is-active", unitName+".service")
	return err == nil && strings.TrimSpace(out) == "active"
}

func Logs(unitName string, userMode bool, follow bool, lines int) error {
	args := []string{"-u", unitName + ".service", "--no-pager"}
	if userMode {
		args = append([]string{"--user"}, args...)
	}
	if lines > 0 {
		args = append(args, "-n", fmt.Sprintf("%d", lines))
	}
	if follow {
		args = append(args, "-f")
	}
	cmd := exec.Command("journalctl", args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	return cmd.Run()
}

func CurrentUsername() string {
	if sudo := strings.TrimSpace(os.Getenv("SUDO_USER")); sudo != "" && sudo != "root" {
		return sudo
	}
	if u, err := user.Current(); err == nil && u.Username != "" {
		return u.Username
	}
	return ""
}

func shellQuote(s string) string {
	if s == "" {
		return s
	}
	if !strings.ContainsAny(s, " \t\"'\\") {
		return s
	}
	return `"` + strings.ReplaceAll(s, `"`, `\"`) + `"`
}

func WhichSystemctl() error {
	if _, err := exec.LookPath("systemctl"); err != nil {
		return fmt.Errorf("systemctl not found (systemd required on this host)")
	}
	return nil
}
