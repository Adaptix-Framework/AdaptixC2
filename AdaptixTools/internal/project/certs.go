package project

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
)

type CertOptions struct {
	Force bool
	Days  int
}

func (l Layout) GenerateCerts(ctx context.Context, out io.Writer, opts CertOptions) error {
	if out == nil {
		out = io.Discard
	}
	if err := os.MkdirAll(l.DistDir, 0o755); err != nil {
		return err
	}
	key := filepath.Join(l.DistDir, "server.rsa.key")
	crt := filepath.Join(l.DistDir, "server.rsa.crt")
	if !opts.Force {
		_, errKey := os.Stat(key)
		_, errCrt := os.Stat(crt)
		if errKey == nil && errCrt == nil {
			fmt.Fprintf(out, "[ok] certs already present in %s (use --force-cert to regenerate)\n", l.DistDir)
			return nil
		}
	}
	if _, err := exec.LookPath("openssl"); err != nil {
		return fmt.Errorf("openssl not found in PATH (needed for --gen-cert)")
	}
	days := opts.Days
	if days <= 0 {
		days = 3650
	}
	args := []string{"req", "-x509", "-nodes", "-newkey", "rsa:2048", "-keyout", key, "-out", crt, "-days", fmt.Sprintf("%d", days), "-subj", "/CN=Server"}
	cmd := exec.CommandContext(ctx, "openssl", args...)
	cmd.Dir = l.DistDir
	cmd.Stdout = out
	cmd.Stderr = out
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("openssl: %w", err)
	}
	_ = os.Chmod(key, 0o600)
	_ = os.Chmod(crt, 0o644)
	fmt.Fprintf(out, "[ok] certs → %s\n", l.DistDir)
	return nil
}
