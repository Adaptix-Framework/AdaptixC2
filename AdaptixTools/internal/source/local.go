package source

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
)

type LocalResolver struct {
	Path string
}

func (r *LocalResolver) Resolve(_ context.Context) ([]Resolved, func(), error) {
	if _, err := os.Stat(filepath.Join(r.Path, specFileName)); err != nil {
		return nil, nil, fmt.Errorf("local source %q has no %s: %w", r.Path, specFileName, err)
	}
	return []Resolved{{
		LocalDir: r.Path,
		Origin:   r.Path,
	}}, nil, nil
}

type BulkResolver struct {
	Root string
}

func (r *BulkResolver) Resolve(_ context.Context) ([]Resolved, func(), error) {
	entries, err := os.ReadDir(r.Root)
	if err != nil {
		return nil, nil, fmt.Errorf("scan %s: %w", r.Root, err)
	}
	var out []Resolved
	for _, e := range entries {
		if !e.IsDir() {
			continue
		}
		child := filepath.Join(r.Root, e.Name())
		if _, err := os.Stat(filepath.Join(child, specFileName)); err != nil {
			continue
		}
		out = append(out, Resolved{
			LocalDir: child,
			Origin:   child,
		})
	}
	if len(out) == 0 {
		return nil, nil, fmt.Errorf("directory %s contains no subdirs with %s", r.Root, specFileName)
	}
	return out, nil, nil
}
