//go:build darwin
// +build darwin

package functions

import (
	"fmt"
	"os"
	"os/user"
	"path/filepath"
	"strings"

	"howett.net/plist"
)

func IsElevated() bool {
	return os.Geteuid() == 0
}

func GetOsVersion() (string, error) {
	f, err := os.Open("/System/Library/CoreServices/SystemVersion.plist")
	if err != nil {
		return "MacOS", nil
	}
	defer f.Close()

	var data map[string]interface{}
	decoder := plist.NewDecoder(f)
	err = decoder.Decode(&data)
	if err != nil {
		return "MacOS", nil
	}

	version, ok := data["ProductVersion"].(string)
	if !ok {
		return "MacOS", nil
	}

	return fmt.Sprintf("MacOS %s", version), nil
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
