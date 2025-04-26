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
