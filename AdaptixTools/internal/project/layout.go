package project

import (
	"fmt"
	"os"
	"path/filepath"

	"axtool/internal/spec"
)

type Layout struct {
	ProjectRoot string
	RepoRoot    string
	ServerDir   string
	ClientDir   string
	DistDir     string
	Spec        spec.ServerSpec
}

func Resolve(projectRootOrSpec string) (Layout, error) {
	l, err := resolveLayout(projectRootOrSpec)
	if err != nil {
		return Layout{}, err
	}
	if _, err := os.Stat(l.ServerDir); err != nil {
		return Layout{}, fmt.Errorf("server_dir %s: %w", l.ServerDir, err)
	}
	return l, nil
}

func ResolveRuntime(projectRootOrSpec string) (Layout, error) {
	return resolveLayout(projectRootOrSpec)
}

func LayoutForSpec(srv spec.ServerSpec, root string) (Layout, error) {
	return layoutFor(srv, root)
}

func resolveLayout(projectRootOrSpec string) (Layout, error) {
	srv, root, err := spec.LoadProject(projectRootOrSpec)
	if err != nil {
		return Layout{}, err
	}
	return layoutFor(srv, root)
}

func layoutFor(srv spec.ServerSpec, root string) (Layout, error) {
	serverDir := srv.ResolvedServerDir(root)

	repoRoot := root
	if srv.RepoRoot != "" {
		repoRoot = resolve(root, srv.RepoRoot)
	}

	clientDir := filepath.Join(root, "AdaptixClient")
	if srv.ClientDir != "" {
		clientDir = resolve(root, srv.ClientDir)
	}

	distDir := filepath.Join(root, "dist")
	if srv.DistDir != "" {
		distDir = resolve(root, srv.DistDir)
	} else if srv.ExtDir != "" {
		relAbs := resolve(root, srv.ExtDir)
		if filepath.Base(relAbs) == "extenders" {
			distDir = filepath.Dir(relAbs)
		}
	}

	return Layout{
		ProjectRoot: root,
		RepoRoot:    repoRoot,
		ServerDir:   serverDir,
		ClientDir:   clientDir,
		DistDir:     distDir,
		Spec:        srv,
	}, nil
}

func resolve(base, rel string) string {
	if filepath.IsAbs(rel) {
		return filepath.Clean(rel)
	}
	return filepath.Join(base, rel)
}
