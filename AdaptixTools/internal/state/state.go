package state

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"

	"gopkg.in/yaml.v3"
)

const FileName = ".installed_plugins.yaml"

type Entry struct {
	Name         string `yaml:"name"`
	Version      string `yaml:"version"`
	Type         string `yaml:"type"`
	Source       string `yaml:"source"`
	Commit       string `yaml:"commit,omitempty"`
	InstalledAt  string `yaml:"installed_at"`
	SourcePath   string `yaml:"source_path"`
	ReleasePath  string `yaml:"release_path"`
	ProfileEntry string `yaml:"profile_entry"`
	SpecVersion  string `yaml:"spec_version,omitempty"`
}

type File struct {
	Plugins []Entry `yaml:"plugins"`
}

func Load(serverDir string) (File, error) {
	path := filepath.Join(serverDir, FileName)
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return File{}, nil
		}
		return File{}, fmt.Errorf("read state %s: %w", path, err)
	}
	var f File
	if err := yaml.Unmarshal(data, &f); err != nil {
		return File{}, fmt.Errorf("parse state %s: %w", path, err)
	}
	return f, nil
}

func Save(serverDir string, f File) error {
	sort.Slice(f.Plugins, func(i, j int) bool {
		return f.Plugins[i].Name < f.Plugins[j].Name
	})
	path := filepath.Join(serverDir, FileName)
	data, err := yaml.Marshal(&f)
	if err != nil {
		return fmt.Errorf("marshal state: %w", err)
	}
	header := []byte("# Managed by axtool. Do not edit by hand.\n")
	if err := os.WriteFile(path, append(header, data...), 0o644); err != nil {
		return fmt.Errorf("write state %s: %w", path, err)
	}
	return nil
}

func (f *File) Find(name string) *Entry {
	for i := range f.Plugins {
		if f.Plugins[i].Name == name {
			return &f.Plugins[i]
		}
	}
	return nil
}

func (f *File) Add(e Entry) {
	for i := range f.Plugins {
		if f.Plugins[i].Name == e.Name {
			f.Plugins[i] = e
			return
		}
	}
	f.Plugins = append(f.Plugins, e)
}

func (f *File) Remove(name string) bool {
	for i := range f.Plugins {
		if f.Plugins[i].Name == name {
			f.Plugins = append(f.Plugins[:i], f.Plugins[i+1:]...)
			return true
		}
	}
	return false
}

func (f *File) Names() []string {
	out := make([]string, 0, len(f.Plugins))
	for _, p := range f.Plugins {
		out = append(out, p.Name)
	}
	sort.Strings(out)
	return out
}
