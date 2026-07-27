package build

import (
	"os"
	"path/filepath"
	"testing"
)

func TestFindConfigRel_AutoDetect(t *testing.T) {
	root := t.TempDir()
	cfg := filepath.Join(root, "config.yaml")
	so := filepath.Join(root, "plugin.so")
	if err := os.WriteFile(cfg, []byte("extender_type: agent\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(so, []byte{}, 0o644); err != nil {
		t.Fatal(err)
	}
	files := []string{cfg, so}
	rel, err := FindConfigRel(files, root, "")
	if err != nil {
		t.Fatal(err)
	}
	if rel != "config.yaml" {
		t.Fatalf("got %q", rel)
	}
	got, err := VerifyRelease(files, root, "")
	if err != nil {
		t.Fatal(err)
	}
	if got != "config.yaml" {
		t.Fatalf("verify got %q", got)
	}
}

func TestFindConfigRel_Prefer(t *testing.T) {
	root := t.TempDir()
	cfg := filepath.Join(root, "myplugin.yaml")
	so := filepath.Join(root, "x.so")
	_ = os.WriteFile(cfg, []byte("x\n"), 0o644)
	_ = os.WriteFile(so, []byte{}, 0o644)
	rel, err := FindConfigRel([]string{cfg, so}, root, "myplugin.yaml")
	if err != nil {
		t.Fatal(err)
	}
	if rel != "myplugin.yaml" {
		t.Fatalf("got %q", rel)
	}
}

func TestFindConfigRel_Nested(t *testing.T) {
	root := t.TempDir()
	sub := filepath.Join(root, "meta")
	_ = os.MkdirAll(sub, 0o755)
	cfg := filepath.Join(sub, "config.yml")
	so := filepath.Join(root, "a.so")
	_ = os.WriteFile(cfg, []byte("x\n"), 0o644)
	_ = os.WriteFile(so, []byte{}, 0o644)
	rel, err := FindConfigRel([]string{cfg, so}, root, "")
	if err != nil {
		t.Fatal(err)
	}
	if rel != "meta/config.yml" {
		t.Fatalf("got %q", rel)
	}
}
