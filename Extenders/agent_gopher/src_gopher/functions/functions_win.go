//go:build windows
// +build windows

package functions

import (
	"fmt"
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

func GetOsVersion() (string, error) {
	modntdll := syscall.NewLazyDLL("ntdll.dll")
	procRtlGetVersion := modntdll.NewProc("RtlGetVersion")

	var info OSVersionInfoEx
	info.dwOSVersionInfoSize = uint32(unsafe.Sizeof(info))

	ret, _, _ := procRtlGetVersion.Call(uintptr(unsafe.Pointer(&info)))
	if ret != 0 {
		return "", fmt.Errorf("RtlGetVersion call failed")
	}

	version := fmt.Sprintf("Windows %d.%d Build %d", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber)
	return version, nil
}

func IsElevated() {
	token := windows.GetCurrentProcessToken()
	return token.IsElevated()
}
