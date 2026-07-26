package fsutil

import (
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
)

func CopyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	info, err := in.Stat()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	out, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, info.Mode())
	if err != nil {
		return err
	}
	defer out.Close()
	_, err = io.Copy(out, in)
	return err
}

func IsSubPath(base, target string) bool {
	if base == target {
		return true
	}
	rel, err := filepath.Rel(base, target)
	if err != nil {
		return false
	}
	if rel == "." {
		return true
	}
	return !strings.HasPrefix(rel, ".."+string(os.PathSeparator)) && rel != ".."
}

func CopyTree(src, dst string, skipDirNames ...string) error {
	dstAbs, err := filepath.Abs(filepath.Clean(dst))
	if err != nil {
		return err
	}
	skip := make(map[string]bool)
	if len(skipDirNames) == 0 {
		skip[".git"] = true
		skip["dist-axtool"] = true
	} else {
		for _, n := range skipDirNames {
			n = strings.TrimSpace(n)
			if n != "" && n != "." && n != ".." {
				skip[n] = true
			}
		}

		skip[".git"] = true
	}
	return filepath.WalkDir(src, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, path)
		if err != nil {
			return err
		}
		if rel == "." {
			return nil
		}
		if d.IsDir() && (d.Type()&fs.ModeSymlink != 0) {
			return filepath.SkipDir
		}
		base := d.Name()
		if d.IsDir() && skip[base] {
			return filepath.SkipDir
		}
		target := filepath.Join(dst, rel)
		targetAbs, err := filepath.Abs(filepath.Clean(target))
		if err != nil {
			return err
		}
		if !IsSubPath(dstAbs, targetAbs) {
			return fmt.Errorf("path traversal blocked: %q escapes %q", rel, dstAbs)
		}
		if d.IsDir() {
			return os.MkdirAll(target, 0o755)
		}
		return CopyFile(path, target)
	})
}
