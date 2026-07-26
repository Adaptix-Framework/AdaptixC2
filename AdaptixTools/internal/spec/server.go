package spec

import "path/filepath"

const ProjectFileName = "adaptix.spec"

type ServerSpec struct {
	ServerVersion  string       `yaml:"server_version"`
	ServerDir      string       `yaml:"server_dir,omitempty"`
	PluginDir      string       `yaml:"plugin_dir"`
	ExtDir         string       `yaml:"ext_dir"`
	Profile        string       `yaml:"profile,omitempty"`
	ExtPrefix      string       `yaml:"ext_prefix,omitempty"`
	AxScriptDir    string       `yaml:"axscript_dir,omitempty"`
	AxScriptPrefix string       `yaml:"axscript_prefix,omitempty"`
	ClientDir      string       `yaml:"client_dir,omitempty"`
	DistDir        string       `yaml:"dist_dir,omitempty"`
	RepoRoot       string       `yaml:"repo_root,omitempty"`
	Packages       []PackageRef `yaml:"packages,omitempty"`
	Systemd        SystemdSpec  `yaml:"systemd,omitempty"`
	Deps           ProjectDeps  `yaml:"deps,omitempty"`
}

type SystemdSpec struct {
	Name     string `yaml:"name,omitempty"`
	User     string `yaml:"user,omitempty"`
	Group    string `yaml:"group,omitempty"`
	Debug    bool   `yaml:"debug,omitempty"`
	UserMode bool   `yaml:"user_mode,omitempty"`
}

type PackageRef struct {
	Source string `yaml:"source"`
	Path   string `yaml:"path,omitempty"`
	Name   string `yaml:"name,omitempty"`

	PluginDir string `yaml:"plugin_dir,omitempty"`
}

func (s ServerSpec) ResolvedServerDir(projectRoot string) string {
	rel := s.ServerDir
	if rel == "" {
		rel = "AdaptixServer"
	}
	return resolveRel(projectRoot, rel, "")
}

func (s ServerSpec) ProfilePath(projectRoot string) string {
	def := "profile.yaml"
	if s.DistDir != "" {
		def = filepath.ToSlash(filepath.Join(s.DistDir, "profile.yaml"))
	} else if s.ExtDir != "" {

		def = filepath.ToSlash(filepath.Join(filepath.Dir(s.ExtDir), "profile.yaml"))
	}
	return resolveRel(projectRoot, s.Profile, def)
}

func (s ServerSpec) ResolvedAxScriptDir(projectRoot string) string {
	rel := s.AxScriptDir
	if rel == "" {
		if s.ExtDir != "" {
			rel = filepath.ToSlash(filepath.Join(filepath.Dir(s.ExtDir), "axscripts"))
		} else {
			rel = "axscripts"
		}
	}
	return resolveRel(projectRoot, rel, "")
}

func (s ServerSpec) ResolvedAxScriptPrefix() string {
	if s.AxScriptPrefix != "" {
		return stringsTrimSlash(s.AxScriptPrefix)
	}
	dir := s.AxScriptDir
	if dir == "" {
		if s.ExtDir != "" {
			dir = filepath.ToSlash(filepath.Join(filepath.Dir(s.ExtDir), "axscripts"))
		} else {
			dir = "axscripts"
		}
	}
	return filepath.Base(filepath.Clean(dir))
}

func resolveRel(base, rel, def string) string {
	if rel == "" {
		rel = def
	}
	if rel == "" {
		return base
	}
	if filepath.IsAbs(rel) {
		return filepath.Clean(rel)
	}
	return filepath.Join(base, rel)
}

func stringsTrimSlash(s string) string {
	for len(s) > 0 && (s[len(s)-1] == '/' || s[len(s)-1] == '\\') {
		s = s[:len(s)-1]
	}
	return s
}
