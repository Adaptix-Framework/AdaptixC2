//go:build darwin
// +build darwin

package functions

import (
	"macos_agent/utils"
	"os"
	"strings"
	"syscall"
	"unsafe"
)

const (
	_CTL_KERN         = 1
	_KERN_PROC        = 14
	_KERN_PROC_PID    = 1
	_P_TRACED         = 0x00000800
	_PT_DENY_ATTACH   = 31
	_SYS_PTRACE       = 26
	_SYS_SYSCTL       = 202
	_SYS_SYSCTLBYNAME = 274
)

const _KINFO_PROC_SIZE = 648
const _P_FLAG_OFFSET = 32

// DenyDebugger calls ptrace(PT_DENY_ATTACH, 0, 0, 0) via raw syscall.
func DenyDebugger() {
	syscall.Syscall6(
		uintptr(_SYS_PTRACE),
		uintptr(_PT_DENY_ATTACH),
		0, 0, 0, 0, 0,
	)
}

// IsDebuggerPresent checks the P_TRACED flag via sysctl(KERN_PROC_PID).
func IsDebuggerPresent() bool {
	var buf [_KINFO_PROC_SIZE]byte
	size := uintptr(len(buf))

	mib := [4]int32{_CTL_KERN, _KERN_PROC, _KERN_PROC_PID, int32(os.Getpid())}

	_, _, errno := syscall.Syscall6(
		uintptr(_SYS_SYSCTL),
		uintptr(unsafe.Pointer(&mib[0])),
		4,
		uintptr(unsafe.Pointer(&buf[0])),
		uintptr(unsafe.Pointer(&size)),
		0,
		0,
	)
	if errno != 0 {
		return false
	}

	pFlag := *(*int32)(unsafe.Pointer(&buf[_P_FLAG_OFFSET]))
	return pFlag&_P_TRACED != 0
}

// sysctlByName is a helper wrapping sysctlbyname via raw syscall.
func sysctlByName(name string) ([]byte, error) {
	nameBytes := append([]byte(name), 0)

	var size uintptr
	_, _, errno := syscall.Syscall6(
		uintptr(_SYS_SYSCTLBYNAME),
		uintptr(unsafe.Pointer(&nameBytes[0])),
		0,
		uintptr(unsafe.Pointer(&size)),
		0,
		0,
		0,
	)
	if errno != 0 || size == 0 {
		return nil, errno
	}

	buf := make([]byte, size)
	_, _, errno = syscall.Syscall6(
		uintptr(_SYS_SYSCTLBYNAME),
		uintptr(unsafe.Pointer(&nameBytes[0])),
		uintptr(unsafe.Pointer(&buf[0])),
		uintptr(unsafe.Pointer(&size)),
		0,
		0,
		0,
	)
	if errno != 0 {
		return nil, errno
	}

	if size > 0 && buf[size-1] == 0 {
		size--
	}
	return buf[:size], nil
}

// GetHWModel returns the hardware model string via sysctlbyname.
func GetHWModel() string {
	data, err := sysctlByName(utils.StrHwModel())
	if err != nil {
		return ""
	}
	return string(data)
}

// IsVirtualMachine checks hw.model for virtualization indicators.
func IsVirtualMachine() bool {
	model := GetHWModel()
	if model == "" {
		return false
	}
	return strings.Contains(model, "Virtual")
}

// IsSandboxed checks if the process is running inside an App Sandbox.
func IsSandboxed() bool {
	return os.Getenv(utils.StrSandboxEnv()) != ""
}

// IsSIPDisabled checks for SIP-disabled indicators via kern.bootargs.
func IsSIPDisabled() bool {
	data, err := sysctlByName(utils.StrKernBootargs())
	if err != nil {
		return false
	}
	return strings.Contains(string(data), utils.StrAmfiBypass())
}

// DetectAnalysisTools checks for the presence of common macOS reversing/analysis tools.
func DetectAnalysisTools() bool {
	toolPaths := []string{
		utils.StrHopper(),
		utils.StrIDA(),
		utils.StrGhidra(),
		utils.StrCharles(),
		utils.StrProxyman(),
		utils.StrWireshark(),
	}
	for _, p := range toolPaths {
		if _, err := os.Stat(p); err == nil {
			return true
		}
	}
	return false
}

// IsAnalysisEnvironment performs a combined check for analysis/debugging indicators.
func IsAnalysisEnvironment() bool {
	if IsDebuggerPresent() {
		return true
	}
	if IsVirtualMachine() {
		return true
	}
	if IsSIPDisabled() {
		return true
	}
	return false
}
