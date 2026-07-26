package install

import (
	"fmt"
	"io"
	"os"
	"path/filepath"

	"axtool/internal/profile"
	"axtool/internal/spec"
	"axtool/internal/state"
	"axtool/internal/workspace"
)

type UninstallOptions struct {
	ProjectRoot string
	ServerDir   string
	ClearSource bool
	Out         io.Writer
}

func Uninstall(opts UninstallOptions, name string) error {
	if opts.ProjectRoot == "" {
		return fmt.Errorf("ProjectRoot is required")
	}
	if opts.ServerDir == "" {
		return fmt.Errorf("ServerDir is required")
	}
	if opts.Out == nil {
		opts.Out = io.Discard
	}
	srv, err := spec.LoadServer(opts.ProjectRoot)
	if err != nil {
		return err
	}
	st, err := state.Load(opts.ServerDir)
	if err != nil {
		return err
	}
	entry := st.Find(name)
	if entry == nil {
		return fmt.Errorf("plugin %q is not installed", name)
	}
	out := opts.Out
	profilePath := srv.ProfilePath(opts.ProjectRoot)

	releaseAbs := abs(opts.ServerDir, entry.ReleasePath)
	if _, err := os.Stat(releaseAbs); err == nil {
		fmt.Fprintf(out, "  → remove release %s\n", releaseAbs)
		if err := os.RemoveAll(releaseAbs); err != nil {
			return fmt.Errorf("remove release: %w", err)
		}
	}

	if opts.ClearSource && entry.Type != spec.TypeAxScript {
		sourceAbs := abs(opts.ServerDir, entry.SourcePath)
		if sourceAbs != releaseAbs {
			if _, err := os.Stat(sourceAbs); err == nil {
				fmt.Fprintf(out, "  → remove source %s\n", sourceAbs)
				if err := os.RemoveAll(sourceAbs); err != nil {
					return fmt.Errorf("remove source: %w", err)
				}
			}
		}
	}

	if entry.Type != spec.TypeAxScript {
		if gw, err := workspace.New(filepath.Join(opts.ServerDir, workspace.GoWorkFileName)); err == nil {
			useEntry := "./" + entry.SourcePath
			if gw.Has(useEntry) {
				fmt.Fprintf(out, "  → go.work: remove %s\n", useEntry)
				if err := gw.Remove(useEntry, false); err != nil {
					return err
				}
				if err := gw.Save(); err != nil {
					return err
				}
			}
		}
	}

	list := profile.ListExtenders
	if entry.Type == spec.TypeAxScript {
		list = profile.ListAxScripts
	}
	if prof, err := profile.New(profilePath); err == nil {
		if prof.HasIn(list, entry.ProfileEntry) {
			fmt.Fprintf(out, "  → %s %s: remove %s\n", profilePath, list, entry.ProfileEntry)
			if err := prof.RemoveFrom(list, entry.ProfileEntry); err != nil {
				return err
			}
			if err := prof.Save(); err != nil {
				return err
			}
		}
	}

	st.Remove(name)
	if err := state.Save(opts.ServerDir, st); err != nil {
		return err
	}

	fmt.Fprintf(out, "[ok] %s uninstalled\n", name)
	return nil
}

func abs(base, rel string) string {
	if filepath.IsAbs(rel) {
		return rel
	}
	return filepath.Join(base, rel)
}
