package install

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"

	"axtool/internal/build"
	"axtool/internal/fsutil"
	"axtool/internal/profile"
	"axtool/internal/spec"
	"axtool/internal/state"
)

type ScriptPlan struct {
	Script       spec.Script
	Origin       string
	InstallDir   string
	ProfileEntry string
	StateEntry   state.Entry
}

func (in *Installer) planScript(s spec.Script, origin string) (ScriptPlan, error) {
	installRoot := in.srv.ResolvedAxScriptDir(in.opts.ProjectRoot)
	installDir := filepath.Join(installRoot, s.Name)

	prefix := in.srv.ResolvedAxScriptPrefix()
	entryRel := filepath.ToSlash(filepath.Join(prefix, s.Name, s.Entry))
	profileEntry := entryRel

	installRel, err := filepath.Rel(in.opts.ServerDir, installDir)
	if err != nil {
		return ScriptPlan{}, err
	}

	return ScriptPlan{
		Script:       s,
		Origin:       origin,
		InstallDir:   installDir,
		ProfileEntry: profileEntry,
		StateEntry: state.Entry{
			Name:         s.Name,
			Version:      s.Version,
			Type:         spec.TypeAxScript,
			Source:       origin,
			InstalledAt:  time.Now().UTC().Format(time.RFC3339),
			SourcePath:   filepath.ToSlash(installRel),
			ReleasePath:  filepath.ToSlash(installRel),
			ProfileEntry: profileEntry,
		},
	}, nil
}

func (in *Installer) precheckScript(p ScriptPlan) error {
	if existing := in.st.Find(p.Script.Name); existing != nil && !in.opts.Force {
		return fmt.Errorf("script %s already installed (version %s); pass --force to reinstall", p.Script.Name, existing.Version)
	}
	info, err := os.Stat(p.InstallDir)
	if err == nil && info.IsDir() {
		if in.st.Find(p.Script.Name) == nil && !in.opts.Force {
			return fmt.Errorf("target axscript dir %s exists but is not tracked by axtool; pass --force to overwrite", p.InstallDir)
		}
	}
	return nil
}

func (in *Installer) installScript(srcDir string, p ScriptPlan, rb *rollback) error {
	s := p.Script
	fmt.Fprintf(in.out, "[install] %s %s (%s)\n", s.Name, s.Version, spec.TypeAxScript)

	if existing := in.st.Find(s.Name); existing != nil && in.opts.Force {
		if err := in.cleanPreviousScript(existing, p); err != nil {
			return err
		}
	}

	kitSrc := srcDir
	if s.Source != "" {
		kitSrc = filepath.Join(srcDir, s.Source)
	}
	if info, err := os.Stat(kitSrc); err != nil || !info.IsDir() {
		return fmt.Errorf("script source dir %s: not a directory", kitSrc)
	}

	entryPath := filepath.Join(kitSrc, s.Entry)
	if _, err := os.Stat(entryPath); err != nil {
		return fmt.Errorf("script entry %s: %w", s.Entry, err)
	}

	if _, err := os.Stat(p.InstallDir); err == nil {
		if err := os.RemoveAll(p.InstallDir); err != nil {
			return fmt.Errorf("clear existing install: %w", err)
		}
	}
	fmt.Fprintf(in.out, "  → copy kit %s → %s\n", kitSrc, p.InstallDir)

	if s.Release.Dir != "" || len(s.Release.Globs) > 0 {
		files, err := build.CollectRelease(kitSrc, s.Release)
		if err != nil {
			return fmt.Errorf("collect script files: %w", err)
		}
		releaseRoot := build.ReleaseRoot(kitSrc, s.Release)
		if err := build.CopyRelease(releaseRoot, p.InstallDir, files); err != nil {
			return err
		}
	} else {
		if err := fsutil.CopyTree(kitSrc, p.InstallDir, ".git"); err != nil {
			return fmt.Errorf("copy kit: %w", err)
		}
	}

	if _, err := os.Stat(filepath.Join(p.InstallDir, s.Entry)); err != nil {
		return fmt.Errorf("after install, entry %s missing in %s", s.Entry, p.InstallDir)
	}

	rb.add(func(out io.Writer) error {
		fmt.Fprintf(out, "  ← remove kit %s\n", p.InstallDir)
		return os.RemoveAll(p.InstallDir)
	})

	if !in.opts.NoProfile {
		profilePath := in.srv.ProfilePath(in.opts.ProjectRoot)
		prof, err := profile.New(profilePath)
		if err != nil {
			return err
		}
		if !prof.HasIn(profile.ListAxScripts, p.ProfileEntry) {
			fmt.Fprintf(in.out, "  → %s axscripts: add %s\n", profilePath, p.ProfileEntry)
			if err := prof.AddTo(profile.ListAxScripts, p.ProfileEntry); err != nil {
				return err
			}
			if err := prof.Save(); err != nil {
				return err
			}
		}
	} else {
		fmt.Fprintf(in.out, "  → profile: skip axscripts register %s (--no-profile)\n", p.ProfileEntry)
	}

	in.st.Add(p.StateEntry)
	if err := state.Save(in.opts.ServerDir, in.st); err != nil {
		return err
	}

	fmt.Fprintf(in.out, "[ok] %s %s → %s (restart server to load)\n", s.Name, s.Version, p.InstallDir)
	return nil
}

func (in *Installer) cleanPreviousScript(existing *state.Entry, p ScriptPlan) error {
	old := abs(in.opts.ServerDir, existing.ReleasePath)
	if old != p.InstallDir {
		if _, err := os.Stat(old); err == nil {
			fmt.Fprintf(in.out, "  → remove previous kit %s\n", old)
			if err := os.RemoveAll(old); err != nil {
				return fmt.Errorf("remove previous kit: %w", err)
			}
		}
	}
	if !in.opts.NoProfile && existing.ProfileEntry != "" && existing.ProfileEntry != p.ProfileEntry {
		prof, err := profile.New(in.srv.ProfilePath(in.opts.ProjectRoot))
		if err == nil && prof.HasIn(profile.ListAxScripts, existing.ProfileEntry) {
			fmt.Fprintf(in.out, "  → profile axscripts: remove stale %s\n", existing.ProfileEntry)
			_ = prof.RemoveFrom(profile.ListAxScripts, existing.ProfileEntry)
			_ = prof.Save()
		}
	}
	return nil
}
