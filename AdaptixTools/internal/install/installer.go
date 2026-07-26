package install

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"time"

	"golang.org/x/mod/semver"

	"axtool/internal/build"
	"axtool/internal/fsutil"
	"axtool/internal/profile"
	"axtool/internal/project"
	"axtool/internal/spec"
	"axtool/internal/state"
	"axtool/internal/workspace"
)

type Options struct {
	ProjectRoot       string
	ServerDir         string
	PluginDirOverride string
	Force             bool
	IgnoreVersion     bool
	OnlyName          string
	InstallHostDeps   bool
	NoProfile         bool
	Out               io.Writer
}

type Installer struct {
	opts Options
	srv  spec.ServerSpec
	st   state.File
	out  io.Writer
}

func New(opts Options) (*Installer, error) {
	if opts.ProjectRoot == "" {
		return nil, errors.New("ProjectRoot is required")
	}
	if opts.ServerDir == "" {
		return nil, errors.New("ServerDir is required")
	}
	if opts.Out == nil {
		opts.Out = io.Discard
	}
	srv, err := spec.LoadServer(opts.ProjectRoot)
	if err != nil {
		return nil, err
	}
	st, err := state.Load(opts.ServerDir)
	if err != nil {
		return nil, err
	}
	return &Installer{opts: opts, srv: srv, st: st, out: opts.Out}, nil
}

type Plan struct {
	Extender     spec.Extender
	Origin       string
	SourceDir    string
	ReleaseDir   string
	UseEntry     string
	ProfileEntry string
	StateEntry   state.Entry
}

func underServerDir(serverDir, absPath string) (rel string, ok bool) {
	serverAbs, err := filepath.Abs(serverDir)
	if err != nil {
		return "", false
	}
	pathAbs, err := filepath.Abs(absPath)
	if err != nil {
		return "", false
	}
	rel, err = filepath.Rel(serverAbs, pathAbs)
	if err != nil {
		return "", false
	}
	if rel == ".." || strings.HasPrefix(rel, ".."+string(filepath.Separator)) {
		return "", false
	}
	return filepath.ToSlash(rel), true
}

func (in *Installer) planExtender(e spec.Extender, origin, pkgDir string) (Plan, error) {
	pluginSrc := pkgDir
	if e.Source != "" {
		pluginSrc = filepath.Join(pkgDir, e.Source)
	}
	pluginSrc, _ = filepath.Abs(pluginSrc)

	releaseDir := in.srv.ExtDir
	if !filepath.IsAbs(releaseDir) {
		releaseDir = filepath.Join(in.opts.ProjectRoot, releaseDir)
	}
	releaseDir = filepath.Join(releaseDir, e.Name)

	extPrefix := in.srv.ExtPrefix
	if extPrefix == "" {
		extPrefix = filepath.Base(filepath.Clean(in.srv.ExtDir))
	}
	extPrefix = strings.TrimSuffix(extPrefix, "/")

	configName := e.Release.Config
	if configName == "" {
		configName = "config.yaml"
	}
	profileEntry := filepath.ToSlash(filepath.Join(extPrefix, e.Name, configName))

	var sourceDir string
	var relDir string

	if rel, ok := underServerDir(in.opts.ServerDir, pluginSrc); ok {
		sourceDir = pluginSrc
		relDir = rel
	} else {
		srcRoot := in.opts.PluginDirOverride
		if srcRoot == "" {
			srcRoot = in.srv.PluginDir
		}
		if srcRoot != "" {
			clean := filepath.ToSlash(filepath.Clean(srcRoot))
			if filepath.IsAbs(clean) {
				return Plan{}, fmt.Errorf("plugin_dir %q must be relative, not absolute", srcRoot)
			}
		}
		if !filepath.IsAbs(srcRoot) {
			srcRoot = filepath.Join(in.opts.ServerDir, srcRoot)
		}
		sourceDir = filepath.Join(srcRoot, e.Name)
		var err error
		relDir, err = filepath.Rel(in.opts.ServerDir, sourceDir)
		if err != nil {
			return Plan{}, err
		}
		relDir = filepath.ToSlash(relDir)
	}

	useEntry := "./" + relDir

	releaseRel, err := filepath.Rel(in.opts.ServerDir, releaseDir)
	if err != nil {
		return Plan{}, err
	}

	return Plan{
		Extender:     e,
		Origin:       origin,
		SourceDir:    sourceDir,
		ReleaseDir:   releaseDir,
		UseEntry:     useEntry,
		ProfileEntry: profileEntry,
		StateEntry: state.Entry{
			Name:         e.Name,
			Version:      e.Version,
			Type:         e.Type,
			Source:       origin,
			InstalledAt:  time.Now().UTC().Format(time.RFC3339),
			SourcePath:   relDir,
			ReleasePath:  filepath.ToSlash(releaseRel),
			ProfileEntry: profileEntry,
		},
	}, nil
}

func (in *Installer) Install(ctx context.Context, srcDir, origin, commit string, pl spec.PluginSpec) error {
	extTargets := pl.Extenders
	scriptTargets := pl.Scripts
	if in.opts.OnlyName != "" {
		extTargets = nil
		scriptTargets = nil
		for _, e := range pl.Extenders {
			if e.Name == in.opts.OnlyName {
				extTargets = []spec.Extender{e}
				break
			}
		}
		for _, s := range pl.Scripts {
			if s.Name == in.opts.OnlyName {
				scriptTargets = []spec.Script{s}
				break
			}
		}
		if len(extTargets) == 0 && len(scriptTargets) == 0 {
			avail := append(namesOf(pl.Extenders), scriptNames(pl.Scripts)...)
			return fmt.Errorf("--name %q not found in spec; available: %v", in.opts.OnlyName, avail)
		}
	}

	plans := make([]Plan, 0, len(extTargets))
	for _, e := range extTargets {
		p, err := in.planExtender(e, origin, srcDir)
		if err != nil {
			return err
		}
		p.StateEntry.Commit = commit
		plans = append(plans, p)
	}
	scriptPlans := make([]ScriptPlan, 0, len(scriptTargets))
	for _, s := range scriptTargets {
		p, err := in.planScript(s, origin)
		if err != nil {
			return err
		}
		p.StateEntry.Commit = commit
		scriptPlans = append(scriptPlans, p)
	}

	for _, p := range plans {
		if err := in.precheck(p); err != nil {
			return err
		}
		if p.Extender.MinServerVersion != "" {
			want := canonicalSemver(p.Extender.MinServerVersion)
			have := canonicalSemver(in.srv.ServerVersion)
			if semver.Compare(want, have) > 0 {
				msg := fmt.Sprintf("%s requires server >= %s, this server is %s",
					p.Extender.Name, p.Extender.MinServerVersion, in.srv.ServerVersion)
				if !in.opts.IgnoreVersion {
					return fmt.Errorf("%s (pass --ignore-version to override)", msg)
				}
				fmt.Fprintf(in.out, "[warn] %s\n", msg)
			}
		}
		for _, req := range p.Extender.Requires {
			if in.st.Find(req) == nil {
				fmt.Fprintf(in.out, "[warn] %s requires %s, which is not installed\n", p.Extender.Name, req)
			}
		}
	}
	for _, p := range scriptPlans {
		if err := in.precheckScript(p); err != nil {
			return err
		}
		if p.Script.MinServerVersion != "" {
			want := canonicalSemver(p.Script.MinServerVersion)
			have := canonicalSemver(in.srv.ServerVersion)
			if semver.Compare(want, have) > 0 {
				msg := fmt.Sprintf("%s requires server >= %s, this server is %s",
					p.Script.Name, p.Script.MinServerVersion, in.srv.ServerVersion)
				if !in.opts.IgnoreVersion {
					return fmt.Errorf("%s (pass --ignore-version to override)", msg)
				}
				fmt.Fprintf(in.out, "[warn] %s\n", msg)
			}
		}
	}

	rb := newRollback()
	statePath := filepath.Join(in.opts.ServerDir, state.FileName)
	goWorkPath := filepath.Join(in.opts.ServerDir, workspace.GoWorkFileName)
	profilePath := in.srv.ProfilePath(in.opts.ProjectRoot)
	rb.snapshotFile(statePath)
	rb.snapshotFile(goWorkPath)
	rb.snapshotFile(profilePath)

	for _, p := range plans {
		if err := in.installOne(ctx, srcDir, p, rb); err != nil {
			fmt.Fprintf(in.out, "[error] %s\n", err)
			fmt.Fprintf(in.out, "[rollback] reverting changes...\n")
			if rerr := rb.apply(in.out); rerr != nil {
				fmt.Fprintf(in.out, "[rollback] FAILED: %v\n", rerr)
			}
			if st, lerr := state.Load(in.opts.ServerDir); lerr == nil {
				in.st = st
			}
			return fmt.Errorf("install %s: %w", p.Extender.Name, err)
		}
	}
	for _, p := range scriptPlans {
		if err := in.installScript(srcDir, p, rb); err != nil {
			fmt.Fprintf(in.out, "[error] %s\n", err)
			fmt.Fprintf(in.out, "[rollback] reverting changes...\n")
			if rerr := rb.apply(in.out); rerr != nil {
				fmt.Fprintf(in.out, "[rollback] FAILED: %v\n", rerr)
			}
			if st, lerr := state.Load(in.opts.ServerDir); lerr == nil {
				in.st = st
			}
			return fmt.Errorf("install %s: %w", p.Script.Name, err)
		}
	}
	return nil
}

func namesOf(es []spec.Extender) []string {
	out := make([]string, 0, len(es))
	for _, e := range es {
		out = append(out, e.Name)
	}
	return out
}

func scriptNames(ss []spec.Script) []string {
	out := make([]string, 0, len(ss))
	for _, s := range ss {
		out = append(out, s.Name)
	}
	return out
}

func (in *Installer) precheck(p Plan) error {
	if existing := in.st.Find(p.Extender.Name); existing != nil && !in.opts.Force {
		return fmt.Errorf("plugin %s already installed (version %s); pass --force to reinstall", p.Extender.Name, existing.Version)
	}
	info, err := os.Stat(p.SourceDir)
	if err == nil && info.IsDir() {
		if in.st.Find(p.Extender.Name) == nil && !in.opts.Force {
			originAbs, _ := filepath.Abs(p.Origin)
			dstAbs, _ := filepath.Abs(p.SourceDir)
			if originAbs == "" || dstAbs == "" || originAbs != dstAbs {
				return fmt.Errorf("target source dir %s exists but is not tracked by axtool; pass --force to overwrite", p.SourceDir)
			}
		}
	}
	return nil
}

func (in *Installer) installOne(ctx context.Context, srcDir string, p Plan, rb *rollback) error {
	e := p.Extender
	fmt.Fprintf(in.out, "[install] %s %s (%s)\n", e.Name, e.Version, e.Type)

	if existing := in.st.Find(e.Name); existing != nil && in.opts.Force {
		if err := in.cleanPrevious(existing, p, rb); err != nil {
			return err
		}
	}

	pluginSrc := srcDir
	if e.Source != "" {
		pluginSrc = filepath.Join(srcDir, e.Source)
	}
	if info, err := os.Stat(pluginSrc); err != nil || !info.IsDir() {
		return fmt.Errorf("plugin source dir %s: not a directory", pluginSrc)
	}

	srcAbs, _ := filepath.Abs(pluginSrc)
	dstAbs, _ := filepath.Abs(p.SourceDir)
	sameTree := srcAbs != "" && dstAbs != "" && srcAbs == dstAbs
	if sameTree {
		if in.opts.Force {
			fmt.Fprintf(in.out, "  → source already in place %s (skip copy; force rebuild)\n", p.SourceDir)
		} else {
			fmt.Fprintf(in.out, "  → source already in place %s (skip copy)\n", p.SourceDir)
		}
	} else {
		if _, err := os.Stat(p.SourceDir); err == nil {
			if err := os.RemoveAll(p.SourceDir); err != nil {
				return fmt.Errorf("clear existing source: %w", err)
			}
		}

		skip := []string{".git", "dist-axtool"}
		if e.Release.Dir != "" {

			skip = append(skip, filepath.Base(filepath.Clean(e.Release.Dir)))
		}
		fmt.Fprintf(in.out, "  → copy source %s → %s\n", pluginSrc, p.SourceDir)
		if err := fsutil.CopyTree(pluginSrc, p.SourceDir, skip...); err != nil {
			return fmt.Errorf("copy source: %w", err)
		}
		rb.add(func(out io.Writer) error {
			fmt.Fprintf(out, "  ← remove source %s\n", p.SourceDir)
			return os.RemoveAll(p.SourceDir)
		})
	}

	gw, err := workspace.New(filepath.Join(in.opts.ServerDir, workspace.GoWorkFileName))
	if err != nil {
		return err
	}
	if !gw.Has(p.UseEntry) {
		fmt.Fprintf(in.out, "  → go.work: add %s\n", p.UseEntry)
		if err := gw.Add(p.UseEntry); err != nil {
			return err
		}
		if err := gw.Save(); err != nil {
			return err
		}
	}

	runner := &build.Runner{
		Dir:       p.SourceDir,
		Out:       in.out,
		LogPrefix: "    ",
	}
	if apt := e.Deps.AptPackages(); len(apt) > 0 && in.opts.InstallHostDeps {
		fmt.Fprintf(in.out, "  → host deps from axtool.spec (%d apt package(s))\n", len(apt))
		if err := project.InstallAptDeps(ctx, in.out, apt); err != nil {
			return fmt.Errorf("plugin deps: %w", err)
		}
	}

	if in.opts.Force {
		if err := in.cleanBuildArtifacts(p); err != nil {
			return err
		}
	}

	fmt.Fprintf(in.out, "  → build in %s\n", p.SourceDir)
	if err := runner.Run(ctx, e.Build); err != nil {
		return err
	}

	files, err := build.CollectRelease(p.SourceDir, e.Release)
	if err != nil {
		return fmt.Errorf("collect release: %w", err)
	}
	releaseRoot := build.ReleaseRoot(p.SourceDir, e.Release)
	configRel, err := build.VerifyRelease(files, releaseRoot, e.Release.Config)
	if err != nil {
		return fmt.Errorf("verify release: %w", err)
	}

	extPrefix := in.srv.ExtPrefix
	if extPrefix == "" {
		extPrefix = filepath.Base(filepath.Clean(in.srv.ExtDir))
	}
	extPrefix = strings.TrimSuffix(extPrefix, "/")
	p.ProfileEntry = filepath.ToSlash(filepath.Join(extPrefix, e.Name, configRel))
	p.StateEntry.ProfileEntry = p.ProfileEntry

	if _, err := os.Stat(p.ReleaseDir); err == nil {
		if err := os.RemoveAll(p.ReleaseDir); err != nil {
			return fmt.Errorf("clear existing release: %w", err)
		}
	}
	fmt.Fprintf(in.out, "  → deploy → %s\n", p.ReleaseDir)
	if err := build.CopyRelease(releaseRoot, p.ReleaseDir, files); err != nil {
		return err
	}
	rb.add(func(out io.Writer) error {
		fmt.Fprintf(out, "  ← remove release %s\n", p.ReleaseDir)
		return os.RemoveAll(p.ReleaseDir)
	})

	if !in.opts.NoProfile {
		profilePath := in.srv.ProfilePath(in.opts.ProjectRoot)
		prof, err := profile.New(profilePath)
		if err != nil {
			return err
		}
		if !prof.Has(p.ProfileEntry) {
			fmt.Fprintf(in.out, "  → %s: add %s\n", profilePath, p.ProfileEntry)
			if err := prof.Add(p.ProfileEntry); err != nil {
				return err
			}
			if err := prof.Save(); err != nil {
				return err
			}
		}
	} else {
		fmt.Fprintf(in.out, "  → profile: skip register %s (--no-profile)\n", p.ProfileEntry)
	}

	in.st.Add(p.StateEntry)
	if err := state.Save(in.opts.ServerDir, in.st); err != nil {
		return err
	}

	fmt.Fprintf(in.out, "[ok] %s %s → %s (restart server to load)\n", e.Name, e.Version, p.ReleaseDir)
	return nil
}

func (in *Installer) cleanPrevious(existing *state.Entry, p Plan, rb *rollback) error {
	oldRel := abs(in.opts.ServerDir, existing.ReleasePath)
	if oldRel != p.ReleaseDir {
		if _, err := os.Stat(oldRel); err == nil {
			fmt.Fprintf(in.out, "  → remove previous release %s\n", oldRel)
			if err := os.RemoveAll(oldRel); err != nil {
				return fmt.Errorf("remove previous release: %w", err)
			}
		}
	}
	oldSrc := abs(in.opts.ServerDir, existing.SourcePath)
	if oldSrc != p.SourceDir {
		if _, err := os.Stat(oldSrc); err == nil {
			fmt.Fprintf(in.out, "  → remove previous source %s\n", oldSrc)
			if err := os.RemoveAll(oldSrc); err != nil {
				return fmt.Errorf("remove previous source: %w", err)
			}
		}
		gw, err := workspace.New(filepath.Join(in.opts.ServerDir, workspace.GoWorkFileName))
		if err == nil {
			oldUse := "./" + existing.SourcePath
			if gw.Has(oldUse) {
				fmt.Fprintf(in.out, "  → go.work: remove stale %s\n", oldUse)
				_ = gw.Remove(oldUse, false)
				_ = gw.Save()
			}
		}
	}
	if !in.opts.NoProfile && existing.ProfileEntry != "" && existing.ProfileEntry != p.ProfileEntry {
		prof, err := profile.New(in.srv.ProfilePath(in.opts.ProjectRoot))
		list := profile.ListExtenders
		if existing.Type == spec.TypeAxScript {
			list = profile.ListAxScripts
		}
		if err == nil && prof.HasIn(list, existing.ProfileEntry) {
			fmt.Fprintf(in.out, "  → profile: remove stale %s\n", existing.ProfileEntry)
			_ = prof.RemoveFrom(list, existing.ProfileEntry)
			_ = prof.Save()
		}
	}
	_ = rb
	return nil
}

func (in *Installer) cleanBuildArtifacts(p Plan) error {
	if st, err := os.Stat(p.ReleaseDir); err == nil && st.IsDir() {
		fmt.Fprintf(in.out, "  → force: clear release %s\n", p.ReleaseDir)
		if err := os.RemoveAll(p.ReleaseDir); err != nil {
			return fmt.Errorf("force clear release: %w", err)
		}
	}

	if p.Extender.Release.Dir == "" {
		return nil
	}
	local := filepath.Join(p.SourceDir, p.Extender.Release.Dir)
	srcClean := filepath.Clean(p.SourceDir)
	localClean := filepath.Clean(local)
	if localClean == srcClean || !strings.HasPrefix(localClean, srcClean+string(filepath.Separator)) {
		return nil
	}
	if st, err := os.Stat(local); err == nil && st.IsDir() {
		fmt.Fprintf(in.out, "  → force: clear build dir %s\n", local)
		if err := os.RemoveAll(local); err != nil {
			return fmt.Errorf("force clear build dir: %w", err)
		}
	}
	return nil
}

func canonicalSemver(s string) string {
	s = strings.TrimSpace(s)
	if s == "" {
		return "v0"
	}
	if !strings.HasPrefix(s, "v") {
		s = "v" + s
	}
	return s
}
