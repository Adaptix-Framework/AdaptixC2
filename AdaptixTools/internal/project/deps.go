package project

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"runtime"
	"strings"
)

func InstallAptDeps(ctx context.Context, out io.Writer, pkgs []string) error {
	if out == nil {
		out = io.Discard
	}
	pkgs = uniqueStrings(pkgs)
	if len(pkgs) == 0 {
		fmt.Fprintln(out, "[deps] no packages listed in spec (skip)")
		return nil
	}
	if runtime.GOOS != "linux" {
		return fmt.Errorf("--install-deps is only implemented on Linux (apt)")
	}
	if _, err := exec.LookPath("apt-get"); err != nil {
		return fmt.Errorf("--install-deps: apt-get not found (Debian/Ubuntu only for now)")
	}

	fmt.Fprintf(out, "[deps] installing %d package(s) via apt-get…\n", len(pkgs))
	for _, p := range pkgs {
		fmt.Fprintf(out, "  · %s\n", p)
	}

	var argv0 string
	var prefix []string
	if os.Geteuid() != 0 {
		if _, err := exec.LookPath("sudo"); err != nil {
			return fmt.Errorf("--install-deps: not root and sudo not found")
		}
		argv0 = "sudo"
		prefix = []string{"apt-get"}
	} else {
		argv0 = "apt-get"
	}

	run := func(args ...string) error {
		full := append(append([]string{}, prefix...), args...)
		cmd := exec.CommandContext(ctx, argv0, full...)
		cmd.Stdout = out
		cmd.Stderr = out
		cmd.Env = append(os.Environ(), "DEBIAN_FRONTEND=noninteractive")
		return cmd.Run()
	}

	if err := run("update"); err != nil {
		return fmt.Errorf("apt-get update: %w", err)
	}
	installArgs := append([]string{"install", "-y", "--no-install-recommends"}, pkgs...)
	if err := run(installArgs...); err != nil {
		return fmt.Errorf("apt-get install: %w", err)
	}
	fmt.Fprintf(out, "[ok] dependencies installed\n")
	return nil
}

func uniqueStrings(in []string) []string {
	seen := make(map[string]struct{}, len(in))
	out := make([]string, 0, len(in))
	for _, s := range in {
		s = strings.TrimSpace(s)
		if s == "" {
			continue
		}
		if _, ok := seen[s]; ok {
			continue
		}
		seen[s] = struct{}{}
		out = append(out, s)
	}
	return out
}
