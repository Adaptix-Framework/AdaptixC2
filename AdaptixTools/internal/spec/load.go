package spec

import (
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"
)

func LoadProject(specPathOrDir string) (ServerSpec, string, error) {
	abs, err := filepath.Abs(specPathOrDir)
	if err != nil {
		return ServerSpec{}, "", err
	}
	info, err := os.Stat(abs)
	if err != nil {
		return ServerSpec{}, "", fmt.Errorf("stat %s: %w", abs, err)
	}
	var file string
	var root string
	if info.IsDir() {
		file = filepath.Join(abs, ProjectFileName)
		root = abs
	} else {
		if filepath.Base(abs) != ProjectFileName {
			return ServerSpec{}, "", fmt.Errorf("%s: expected a file named %s", abs, ProjectFileName)
		}
		file = abs
		root = filepath.Dir(abs)
	}
	s, err := loadProjectFile(file)
	if err != nil {
		return ServerSpec{}, "", err
	}
	return s, root, nil
}

func LoadServer(projectRoot string) (ServerSpec, error) {
	s, _, err := LoadProject(projectRoot)
	return s, err
}

func loadProjectFile(path string) (ServerSpec, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return ServerSpec{}, fmt.Errorf("read project spec %s: %w", path, err)
	}
	var s ServerSpec
	if err := yaml.Unmarshal(data, &s); err != nil {
		return ServerSpec{}, fmt.Errorf("parse project spec %s: %w", path, err)
	}
	if err := s.Validate(); err != nil {
		return ServerSpec{}, fmt.Errorf("%s: %w", path, err)
	}
	return s, nil
}

func LoadPlugin(repoDirOrSpec string) (PluginSpec, error) {
	path := repoDirOrSpec
	info, err := os.Stat(repoDirOrSpec)
	if err != nil {
		return PluginSpec{}, fmt.Errorf("stat plugin path %s: %w", repoDirOrSpec, err)
	}
	if info.IsDir() {
		path = filepath.Join(repoDirOrSpec, PluginFileName)
	} else if filepath.Base(repoDirOrSpec) != PluginFileName {
		return PluginSpec{}, fmt.Errorf("plugin path %s is a file but not named %s", repoDirOrSpec, PluginFileName)
	}
	return loadPluginFile(path)
}

func loadPluginFile(path string) (PluginSpec, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return PluginSpec{}, fmt.Errorf("read plugin spec %s: %w", path, err)
	}
	var p PluginSpec
	if err := yaml.Unmarshal(data, &p); err != nil {
		return PluginSpec{}, fmt.Errorf("parse plugin spec %s: %w", path, err)
	}
	if err := p.Validate(); err != nil {
		return PluginSpec{}, fmt.Errorf("%s: %w", path, err)
	}
	return p, nil
}
