package spec

import (
	"errors"
	"fmt"
	"path/filepath"
	"strings"
)

func (s ServerSpec) Validate() error {
	var errs []string
	if s.ServerVersion == "" {
		errs = append(errs, "server_version is required")
	}
	if s.PluginDir == "" {
		errs = append(errs, "plugin_dir is required")
	}
	if s.ExtDir == "" {
		errs = append(errs, "ext_dir is required")
	}
	if s.ServerDir != "" {
		if err := checkRelativePath("server_dir", s.ServerDir); err != nil {
			errs = append(errs, err.Error())
		}
	}

	if err := checkSafeRelPath("plugin_dir", s.PluginDir); err != nil {
		errs = append(errs, err.Error())
	}
	if err := checkRelativePath("ext_dir", s.ExtDir); err != nil {
		errs = append(errs, err.Error())
	}
	if s.Profile != "" {
		if err := checkRelativePath("profile", s.Profile); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.ExtPrefix != "" {
		if err := checkSafeRelPath("ext_prefix", s.ExtPrefix); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.AxScriptDir != "" {
		if err := checkRelativePath("axscript_dir", s.AxScriptDir); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.AxScriptPrefix != "" {
		if err := checkSafeRelPath("axscript_prefix", s.AxScriptPrefix); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.RepoRoot != "" {
		if err := checkRelativePath("repo_root", s.RepoRoot); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.ClientDir != "" {
		if err := checkRelativePath("client_dir", s.ClientDir); err != nil {
			errs = append(errs, err.Error())
		}
	}
	if s.DistDir != "" {
		if err := checkRelativePath("dist_dir", s.DistDir); err != nil {
			errs = append(errs, err.Error())
		}
	}
	for i, p := range s.Packages {
		if strings.TrimSpace(p.Source) == "" {
			errs = append(errs, fmt.Sprintf("packages[%d]: source is required", i))
		}
		if p.Name != "" && !IsSafeName(p.Name) {
			errs = append(errs, fmt.Sprintf("packages[%d]: invalid name %q", i, p.Name))
		}
	}
	if len(errs) > 0 {
		return errors.New("invalid project adaptix.spec: " + strings.Join(errs, "; "))
	}
	return nil
}

func (p PluginSpec) Validate() error {
	if len(p.Extenders) == 0 && len(p.Scripts) == 0 {
		return errors.New("plugin spec has no extenders or scripts")
	}
	seen := make(map[string]bool)

	for i, e := range p.Extenders {
		pfx := fmt.Sprintf("extenders[%d]", i)
		if e.Name == "" {
			return fmt.Errorf("%s: name is required", pfx)
		}
		if !IsSafeName(e.Name) {
			return fmt.Errorf("%s: name %q must match [a-z0-9][a-z0-9_-]*", pfx, e.Name)
		}
		if seen[e.Name] {
			return fmt.Errorf("%s: duplicate name %q", pfx, e.Name)
		}
		seen[e.Name] = true

		if e.Version == "" {
			return fmt.Errorf("%s (%s): version is required", pfx, e.Name)
		}
		switch e.Type {
		case "listener", "agent", "service":
		default:
			return fmt.Errorf("%s (%s): type must be listener|agent|service, got %q", pfx, e.Name, e.Type)
		}
		if len(e.Build) == 0 {
			return fmt.Errorf("%s (%s): build must list at least one command", pfx, e.Name)
		}
		if e.Release.Dir == "" && len(e.Release.Globs) == 0 {
			return fmt.Errorf("%s (%s): release must specify dir or globs", pfx, e.Name)
		}
		if e.Release.Dir != "" && len(e.Release.Globs) > 0 {
			return fmt.Errorf("%s (%s): release must specify dir OR globs, not both", pfx, e.Name)
		}
		if e.Source != "" {
			if err := checkSafeRelPath("source", e.Source); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, e.Name, err)
			}
		}
		if e.Release.Dir != "" {
			if err := checkSafeRelPath("release.dir", e.Release.Dir); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, e.Name, err)
			}
		}
		if e.Release.Config != "" {
			if err := checkSafeRelPath("release.config", e.Release.Config); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, e.Name, err)
			}
		}
		for _, g := range e.Release.Globs {
			if err := checkSafeGlob("release.globs", g); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, e.Name, err)
			}
		}
	}

	for i, s := range p.Scripts {
		pfx := fmt.Sprintf("scripts[%d]", i)
		if s.Name == "" {
			return fmt.Errorf("%s: name is required", pfx)
		}
		if !IsSafeName(s.Name) {
			return fmt.Errorf("%s: name %q must match [a-z0-9][a-z0-9_-]*", pfx, s.Name)
		}
		if seen[s.Name] {
			return fmt.Errorf("%s: duplicate name %q (conflicts with extender or script)", pfx, s.Name)
		}
		seen[s.Name] = true
		if s.Version == "" {
			return fmt.Errorf("%s (%s): version is required", pfx, s.Name)
		}
		if s.Entry == "" {
			return fmt.Errorf("%s (%s): entry (.axs path) is required", pfx, s.Name)
		}
		if err := checkSafeRelPath("entry", s.Entry); err != nil {
			return fmt.Errorf("%s (%s): %w", pfx, s.Name, err)
		}
		if !strings.HasSuffix(strings.ToLower(s.Entry), ".axs") {
			return fmt.Errorf("%s (%s): entry must be a .axs file, got %q", pfx, s.Name, s.Entry)
		}
		if s.Source != "" {
			if err := checkSafeRelPath("source", s.Source); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, s.Name, err)
			}
		}
		if s.Release.Dir != "" && len(s.Release.Globs) > 0 {
			return fmt.Errorf("%s (%s): release must specify dir OR globs, not both", pfx, s.Name)
		}
		if s.Release.Dir != "" {
			if err := checkSafeRelPath("release.dir", s.Release.Dir); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, s.Name, err)
			}
		}
		for _, g := range s.Release.Globs {
			if err := checkSafeGlob("release.globs", g); err != nil {
				return fmt.Errorf("%s (%s): %w", pfx, s.Name, err)
			}
		}
	}
	return nil
}

func IsSafeName(s string) bool {
	if s == "" {
		return false
	}
	for i, r := range s {
		switch {
		case r >= 'a' && r <= 'z':
		case r >= '0' && r <= '9':
		case (r == '_' || r == '-') && i > 0:
		default:
			return false
		}
	}
	return true
}

func checkSafeRelPath(field, p string) error {
	if p == "" {
		return nil
	}
	clean := filepath.ToSlash(filepath.Clean(p))
	if filepath.IsAbs(clean) {
		return fmt.Errorf("%s: %q must be relative, not absolute", field, p)
	}
	for _, seg := range strings.Split(clean, "/") {
		if seg == ".." {
			return fmt.Errorf("%s: %q must not contain '..'", field, p)
		}
	}
	return nil
}

func checkRelativePath(field, p string) error {
	if p == "" {
		return nil
	}
	clean := filepath.ToSlash(filepath.Clean(p))
	if filepath.IsAbs(clean) {
		return fmt.Errorf("%s: %q must be relative, not absolute", field, p)
	}
	return nil
}

func checkSafeGlob(field, g string) error {
	if g == "" {
		return fmt.Errorf("%s: empty glob", field)
	}
	clean := filepath.ToSlash(filepath.Clean(g))
	if filepath.IsAbs(clean) {
		return fmt.Errorf("%s: %q must be relative, not absolute", field, g)
	}
	for _, seg := range strings.Split(clean, "/") {
		if seg == ".." {
			return fmt.Errorf("%s: %q must not contain '..'", field, g)
		}
	}
	return nil
}
