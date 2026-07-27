package build

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"strings"

	"axtool/internal/fsutil"
	"axtool/internal/spec"
)

func CollectRelease(srcDir string, rel spec.Release) ([]string, error) {
	if rel.Dir != "" {
		return collectDir(srcDir, rel.Dir)
	}
	if len(rel.Globs) > 0 {
		return collectGlobs(srcDir, rel.Globs)
	}
	return nil, errors.New("release has neither dir nor globs")
}

func ReleaseRoot(srcDir string, rel spec.Release) string {
	if rel.Dir != "" {
		return filepath.Join(srcDir, rel.Dir)
	}
	return srcDir
}

func collectDir(srcDir, sub string) ([]string, error) {
	abs, err := filepath.Abs(filepath.Join(srcDir, sub))
	if err != nil {
		return nil, err
	}
	info, err := os.Stat(abs)
	if err != nil {
		return nil, fmt.Errorf("release dir %s: %w", sub, err)
	}
	if !info.IsDir() {
		return nil, fmt.Errorf("release dir %s is not a directory", sub)
	}
	var files []string
	err = filepath.WalkDir(abs, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		files = append(files, path)
		return nil
	})
	if err != nil {
		return nil, fmt.Errorf("walk release dir %s: %w", sub, err)
	}
	if len(files) == 0 {
		return nil, fmt.Errorf("release dir %s is empty", sub)
	}
	return files, nil
}

func collectGlobs(srcDir string, globs []string) ([]string, error) {
	var out []string
	seen := make(map[string]bool)
	for _, g := range globs {
		matches, err := filepath.Glob(filepath.Join(srcDir, g))
		if err != nil {
			return nil, fmt.Errorf("release glob %q: %w", g, err)
		}
		if len(matches) == 0 {
			return nil, fmt.Errorf("release glob %q matched nothing in %s", g, srcDir)
		}
		for _, m := range matches {
			info, err := os.Stat(m)
			if err != nil {
				return nil, err
			}
			if info.IsDir() {
				err := filepath.WalkDir(m, func(path string, d fs.DirEntry, err error) error {
					if err != nil {
						return err
					}
					if d.IsDir() {
						return nil
					}
					if !seen[path] {
						seen[path] = true
						out = append(out, path)
					}
					return nil
				})
				if err != nil {
					return nil, err
				}
			} else if !seen[m] {
				seen[m] = true
				out = append(out, m)
			}
		}
	}
	if len(out) == 0 {
		return nil, errors.New("release globs matched no files")
	}
	return out, nil
}

func FindConfigRel(files []string, releaseRoot, prefer string) (string, error) {
	rootAbs, err := filepath.Abs(releaseRoot)
	if err != nil {
		return "", err
	}
	prefer = filepath.ToSlash(filepath.Clean(strings.TrimSpace(prefer)))
	if prefer == "." {
		prefer = ""
	}

	type cand struct {
		rel   string
		depth int
	}
	var matches []cand

	for _, f := range files {
		rel, err := filepath.Rel(rootAbs, f)
		if err != nil {
			continue
		}
		if strings.HasPrefix(rel, "..") {
			continue
		}
		relSlash := filepath.ToSlash(rel)
		base := filepath.Base(rel)

		if prefer != "" {
			if relSlash == prefer || base == prefer || strings.EqualFold(relSlash, prefer) {
				return relSlash, nil
			}
			continue
		}

		low := strings.ToLower(base)
		if low == "config.yaml" || low == "config.yml" {
			depth := strings.Count(relSlash, "/")
			matches = append(matches, cand{rel: relSlash, depth: depth})
		}
	}

	if prefer != "" {
		return "", fmt.Errorf("release does not contain config %q (set release.config or include it in release.dir/globs)", prefer)
	}
	if len(matches) == 0 {
		return "", errors.New("release does not contain a config file (config.yaml / config.yml, or set release.config)")
	}

	best := matches[0]
	for _, m := range matches[1:] {
		if m.depth < best.depth {
			best = m
			continue
		}
		if m.depth == best.depth {
			if strings.HasSuffix(strings.ToLower(m.rel), "config.yaml") &&
				!strings.HasSuffix(strings.ToLower(best.rel), "config.yaml") {
				best = m
			}
		}
	}
	return best.rel, nil
}

func VerifyRelease(files []string, releaseRoot, preferConfig string) (configRel string, err error) {
	hasSO := false
	for _, f := range files {
		if strings.HasSuffix(strings.ToLower(filepath.Base(f)), ".so") {
			hasSO = true
			break
		}
	}
	if !hasSO {
		return "", errors.New("release does not contain any .so file")
	}
	configRel, err = FindConfigRel(files, releaseRoot, preferConfig)
	if err != nil {
		return "", err
	}
	return configRel, nil
}

func CopyRelease(srcRoot, dstDir string, files []string) error {
	if err := os.MkdirAll(dstDir, 0o755); err != nil {
		return fmt.Errorf("mkdir release %s: %w", dstDir, err)
	}
	srcRootAbs, err := filepath.Abs(srcRoot)
	if err != nil {
		return err
	}
	for _, f := range files {
		rel, err := filepath.Rel(srcRootAbs, f)
		if err != nil {
			return fmt.Errorf("rel(%s, %s): %w", srcRootAbs, f, err)
		}
		if strings.HasPrefix(rel, "..") {
			return fmt.Errorf("release file %s is outside source root %s", f, srcRootAbs)
		}
		dst := filepath.Join(dstDir, rel)
		if err := fsutil.CopyFile(f, dst); err != nil {
			return fmt.Errorf("copy %s → %s: %w", rel, dst, err)
		}
	}
	return nil
}
