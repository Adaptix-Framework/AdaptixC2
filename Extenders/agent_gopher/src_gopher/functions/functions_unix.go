//go:build linux
// +build linux

package functions

import (
	"fmt"
	"os"
	"os/user"
	"path/filepath"
	"strings"
)

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
