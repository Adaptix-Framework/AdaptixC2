package scaffold

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"strings"

	"axtool/internal/fsutil"
	srcres "axtool/internal/source"
	"axtool/internal/spec"
)

const (
	DefaultSource = "github.com/Adaptix-Framework/templates-extender"

	DefaultRef = "main"
)

type Type string

const (
	TypeAgent    Type = "agent"
	TypeListener Type = "listener"
	TypeService  Type = "service"
	TypeAxScript Type = "axscript"
)

type Options struct {
	Type Type

	Name string

	TargetDir string

	Source string

	Protocol string
}

func templateSubdir(t Type) (string, error) {
	switch t {
	case TypeAgent:
		return "agent_template", nil
	case TypeListener:
		return "listener_template", nil
	case TypeService:
		return "service_template", nil
	default:
		return "", fmt.Errorf("no remote template for type %q", t)
	}
}

func Run(ctx context.Context, opts Options) error {
	if opts.Name == "" || !spec.IsSafeName(opts.Name) {
		return fmt.Errorf("invalid name %q: must match [a-z0-9][a-z0-9_-]*", opts.Name)
	}
	if opts.TargetDir == "" {
		return fmt.Errorf("TargetDir is required")
	}
	if opts.Protocol == "" {
		opts.Protocol = "custom"
	}
	if !spec.IsSafeName(opts.Protocol) {

		for _, r := range opts.Protocol {
			if (r < 'a' || r > 'z') && (r < '0' || r > '9') && r != '_' && r != '-' {
				return fmt.Errorf("invalid protocol %q", opts.Protocol)
			}
		}
	}

	if opts.Type == TypeAxScript {
		return scaffoldAxScript(opts)
	}

	subdir, err := templateSubdir(opts.Type)
	if err != nil {
		return err
	}

	srcRoot, cleanup, err := resolveTemplatesRoot(ctx, opts.Source)
	if err != nil {
		return err
	}
	if cleanup != nil {
		defer cleanup()
	}

	tplDir := filepath.Join(srcRoot, subdir)
	if st, err := os.Stat(tplDir); err != nil || !st.IsDir() {
		return fmt.Errorf("template %q not found in %s (expected subdir %s)", opts.Type, srcRoot, subdir)
	}

	if err := os.MkdirAll(opts.TargetDir, 0o755); err != nil {
		return err
	}

	if err := ensureEmptyOrOnlyDot(opts.TargetDir); err != nil {
		return err
	}

	if err := fsutil.CopyTree(tplDir, opts.TargetDir, ".git", "dist", "dist-axtool"); err != nil {
		return fmt.Errorf("copy template: %w", err)
	}

	wm, err := randomHex(4)
	if err != nil {
		return err
	}
	repl := replacements(opts, wm)
	if err := replaceInTree(opts.TargetDir, repl); err != nil {
		return err
	}
	if err := writePluginSpec(opts); err != nil {
		return err
	}
	return nil
}

func resolveTemplatesRoot(ctx context.Context, source string) (root string, cleanup func(), err error) {
	source = strings.TrimSpace(source)
	if source == "" {
		source = DefaultSource + "@" + DefaultRef
	} else if !strings.Contains(source, "@") && isGitish(source) {
		source = source + "@" + DefaultRef
	}

	if !isGitish(source) {
		abs, err := filepath.Abs(source)
		if err != nil {
			return "", nil, err
		}
		if st, err := os.Stat(abs); err != nil || !st.IsDir() {
			return "", nil, fmt.Errorf("template source %q: not a directory", source)
		}

		return abs, nil, nil
	}

	res, err := srcres.New(source)
	if err != nil {
		return "", nil, fmt.Errorf("template source: %w", err)
	}
	resolved, cleanup, err := res.Resolve(ctx)
	if err != nil {
		return "", nil, fmt.Errorf("fetch templates: %w", err)
	}
	if len(resolved) == 0 {
		if cleanup != nil {
			cleanup()
		}
		return "", nil, fmt.Errorf("empty template clone")
	}
	return resolved[0].LocalDir, cleanup, nil
}

func isGitish(s string) bool {
	base := s
	if i := strings.LastIndex(s, "@"); i > 0 {

		if strings.Contains(s[:i], "/") {
			base = s[:i]
		}
	}
	return strings.HasPrefix(base, "github.com/") ||
		strings.HasPrefix(base, "https://") ||
		strings.HasPrefix(base, "http://") ||
		strings.HasPrefix(base, "git@") ||
		strings.HasSuffix(base, ".git")
}

func replacements(opts Options, watermark string) map[string]string {
	name := opts.Name
	so := name + ".so"
	m := map[string]string{
		"_SO_FILE_HERE_": so,
	}
	switch opts.Type {
	case TypeAgent:
		m["_AGENT_"] = name
		m["_RANDOM_HEX_8_"] = watermark

		m["adaptix_agent_NAME"] = "adaptix_agent_" + sanitizeMod(name)
	case TypeListener:
		m["_LISTENER_"] = name
		m["_PROTOCOL_"] = opts.Protocol
		m["adaptix_listener_NAME_PROTOCOL"] = "adaptix_listener_" + sanitizeMod(name) + "_" + sanitizeMod(opts.Protocol)

		m["listener_LISTENER_"] = "listener_" + name
	case TypeService:
		m["_SERVICE_"] = name
		m["adaptix_service_NAME_PROTOCOL"] = "adaptix_service_" + sanitizeMod(name)
	}
	return m
}

func sanitizeMod(s string) string {
	s = strings.ReplaceAll(s, "-", "_")
	return s
}

func replaceInTree(root string, repl map[string]string) error {
	return filepath.WalkDir(root, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			if d.Name() == ".git" || d.Name() == "dist" {
				return filepath.SkipDir
			}
			return nil
		}

		name := d.Name()
		if strings.HasSuffix(name, ".so") || strings.HasSuffix(name, ".sum") {

			if strings.HasSuffix(name, ".so") {
				return nil
			}
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		s := string(data)
		orig := s
		for old, neu := range repl {
			if old == "" {
				continue
			}
			s = strings.ReplaceAll(s, old, neu)
		}
		if s != orig {
			mode := fs.FileMode(0o644)
			if info, err := d.Info(); err == nil {
				mode = info.Mode()
			}
			return os.WriteFile(path, []byte(s), mode)
		}
		return nil
	})
}

func writePluginSpec(opts Options) error {
	so := opts.Name + ".so"
	body := fmt.Sprintf(`extenders:
  - name: %s
    version: 0.1.0
    type: %s
    description: "TODO: describe this plugin"
    author: TODO
    min_server_version: "v2.0"

    build:
      - make

    release:
      dir: dist/
`, opts.Name, opts.Type)

	_ = so
	return os.WriteFile(filepath.Join(opts.TargetDir, spec.PluginFileName), []byte(body), 0o644)
}

func scaffoldAxScript(opts Options) error {
	if err := os.MkdirAll(opts.TargetDir, 0o755); err != nil {
		return err
	}
	if err := ensureEmptyOrOnlyDot(opts.TargetDir); err != nil {
		return err
	}
	entry := opts.Name + ".axs"
	specBody := fmt.Sprintf(`scripts:
  - name: %s
    version: 0.1.0
    description: "TODO: describe this AxScript kit"
    author: TODO
    min_server_version: "v2.0"
    entry: %s
`, opts.Name, entry)
	axsBody := fmt.Sprintf(`var metadata = {
    name: "%s",
    description: "TODO",
    nosave: false
};
`, opts.Name)
	if err := os.WriteFile(filepath.Join(opts.TargetDir, spec.PluginFileName), []byte(specBody), 0o644); err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(opts.TargetDir, entry), []byte(axsBody), 0o644)
}

func ensureEmptyOrOnlyDot(dir string) error {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return err
	}
	for _, e := range entries {

		return fmt.Errorf("target directory %s is not empty (%s)", dir, e.Name())
	}
	return nil
}

func randomHex(nBytes int) (string, error) {
	b := make([]byte, nBytes)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}
