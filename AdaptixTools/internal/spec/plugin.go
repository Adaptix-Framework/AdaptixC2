package spec

const PluginFileName = "axtool.spec"

type PluginSpec struct {
	Extenders []Extender `yaml:"extenders,omitempty"`
	Scripts   []Script   `yaml:"scripts,omitempty"`
}

type Extender struct {
	Name             string   `yaml:"name"`
	Version          string   `yaml:"version"`
	Type             string   `yaml:"type"`
	Description      string   `yaml:"description,omitempty"`
	Author           string   `yaml:"author,omitempty"`
	MinServerVersion string   `yaml:"min_server_version,omitempty"`
	Requires         []string `yaml:"requires,omitempty"`
	Source           string   `yaml:"source,omitempty"`

	Deps    HostDeps `yaml:"deps,omitempty"`
	Build   []string `yaml:"build"`
	Release Release  `yaml:"release"`
}

type Script struct {
	Name             string `yaml:"name"`
	Version          string `yaml:"version"`
	Description      string `yaml:"description,omitempty"`
	Author           string `yaml:"author,omitempty"`
	MinServerVersion string `yaml:"min_server_version,omitempty"`
	Source           string `yaml:"source,omitempty"`

	Entry string `yaml:"entry"`

	Release Release `yaml:"release,omitempty"`
}

type Release struct {
	Dir   string   `yaml:"dir,omitempty"`
	Globs []string `yaml:"globs,omitempty"`

	Config string `yaml:"config,omitempty"`
}

const TypeAxScript = "axscript"
