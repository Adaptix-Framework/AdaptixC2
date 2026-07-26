package install

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
)

type rollback struct {
	fileSnapshots map[string][]byte
	fileExisted   map[string]bool
	actions       []revertFn
}

type revertFn func(out io.Writer) error

func newRollback() *rollback {
	return &rollback{
		fileSnapshots: map[string][]byte{},
		fileExisted:   map[string]bool{},
	}
}

func (r *rollback) snapshotFile(path string) {
	if _, ok := r.fileSnapshots[path]; ok {
		return
	}
	data, err := os.ReadFile(path)
	if err != nil {
		r.fileSnapshots[path] = nil
		r.fileExisted[path] = false
		r.actions = append(r.actions, func(out io.Writer) error {
			fmt.Fprintf(out, "  ← remove %s (was absent)\n", path)
			_ = os.Remove(path)
			return nil
		})
		return
	}
	snap := make([]byte, len(data))
	copy(snap, data)
	r.fileSnapshots[path] = snap
	r.fileExisted[path] = true
	r.actions = append(r.actions, func(out io.Writer) error {
		fmt.Fprintf(out, "  ← restore %s (%d bytes)\n", path, len(snap))
		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			return err
		}
		return os.WriteFile(path, snap, 0o644)
	})
}

func (r *rollback) add(fn revertFn) {
	r.actions = append(r.actions, fn)
}

func (r *rollback) apply(out io.Writer) error {
	var firstErr error
	for i := len(r.actions) - 1; i >= 0; i-- {
		if err := r.actions[i](out); err != nil && firstErr == nil {
			firstErr = err
		}
	}
	r.actions = nil
	return firstErr
}
