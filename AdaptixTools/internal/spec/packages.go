package spec

import (
	"fmt"
	"os"
	"strings"

	"gopkg.in/yaml.v3"
)

type PackagesFile struct {
	Packages []PackageRef `yaml:"packages"`
}

func LoadPackagesFile(path string) ([]PackageRef, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read packages file %s: %w", path, err)
	}

	var wrapped PackagesFile
	if err := yaml.Unmarshal(data, &wrapped); err == nil && len(wrapped.Packages) > 0 {
		return validatePackageRefs(wrapped.Packages)
	}
	var bare []PackageRef
	if err := yaml.Unmarshal(data, &bare); err == nil && len(bare) > 0 {
		return validatePackageRefs(bare)
	}

	var strs []string
	if err := yaml.Unmarshal(data, &strs); err == nil && len(strs) > 0 {
		out := make([]PackageRef, 0, len(strs))
		for _, s := range strs {
			s = strings.TrimSpace(s)
			if s == "" {
				continue
			}
			out = append(out, PackageRef{Source: s})
		}
		return validatePackageRefs(out)
	}
	return nil, fmt.Errorf("packages file %s: no packages found (use packages: [{source: ...}])", path)
}

func validatePackageRefs(refs []PackageRef) ([]PackageRef, error) {
	out := make([]PackageRef, 0, len(refs))
	for i, p := range refs {
		p.Source = strings.TrimSpace(p.Source)
		p.Path = strings.TrimSpace(p.Path)
		if p.Source == "" {
			return nil, fmt.Errorf("packages[%d]: source is required", i)
		}
		if p.Name != "" && !IsSafeName(p.Name) {
			return nil, fmt.Errorf("packages[%d]: invalid name %q", i, p.Name)
		}
		if p.Path != "" {

			clean := strings.ReplaceAll(p.Path, "\\", "/")
			if strings.HasPrefix(clean, "/") || strings.Contains(clean, "..") {
				return nil, fmt.Errorf("packages[%d]: path %q must be a relative subdir without '..'", i, p.Path)
			}
			p.Path = strings.Trim(clean, "/")
		}
		out = append(out, p)
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("packages list is empty")
	}
	return out, nil
}
