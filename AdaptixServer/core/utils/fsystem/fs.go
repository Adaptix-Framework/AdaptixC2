package fsystem

import (
	"os"
	"path/filepath"
)

func ResolveRealPath(path string) (string, error) {
	info, err := os.Lstat(path)
	if err != nil {
		return filepath.Clean(path), nil
	}

	if info.Mode()&os.ModeSymlink != 0 {
		resolved, err := filepath.EvalSymlinks(path)
		if err != nil {
			return "", err
		}
		return filepath.Clean(resolved), nil
	}

	return filepath.Clean(path), nil
}
