package source

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"
)

type GitResolver struct {
	Source string
}

var shaRE = regexp.MustCompile(`(?i)^[0-9a-f]{7,40}$`)

func (r *GitResolver) Resolve(ctx context.Context) ([]Resolved, func(), error) {
	url, ref := splitRef(r.Source)

	tempDir, err := os.MkdirTemp("", "axtool-clone-*")
	if err != nil {
		return nil, nil, fmt.Errorf("create temp dir: %w", err)
	}
	cleanup := func() { _ = os.RemoveAll(tempDir) }

	if err := cloneGit(ctx, url, ref, tempDir); err != nil {
		cleanup()
		return nil, nil, err
	}

	commit, _ := gitHead(ctx, tempDir)

	origin := r.Source
	if ref != "" {
		origin = fmt.Sprintf("%s@%s", url, ref)
	}

	return []Resolved{{
		LocalDir: tempDir,
		Origin:   origin,
		Commit:   commit,
	}}, cleanup, nil
}

func cloneGit(ctx context.Context, url, ref, dest string) error {
	url = normalizeGitURL(url)

	run := func(cmd *exec.Cmd) error {
		var stderr strings.Builder
		cmd.Stdout = nil
		cmd.Stderr = &stderr
		if err := cmd.Run(); err != nil {
			msg := strings.TrimSpace(stderr.String())
			if msg != "" {
				return fmt.Errorf("%w\n----- git log -----\n%s\n----- end -----", err, msg)
			}
			return err
		}
		return nil
	}

	if ref != "" && shaRE.MatchString(ref) {
		cmd := exec.CommandContext(ctx, "git", "clone", "-q", url, dest)
		if err := run(cmd); err != nil {
			return fmt.Errorf("git clone %s: %w", redact(url), err)
		}
		co := exec.CommandContext(ctx, "git", "-C", dest, "checkout", "--detach", "-q", ref)
		if err := run(co); err != nil {
			return fmt.Errorf("git checkout %s: %w", ref, err)
		}
		return nil
	}

	args := []string{"clone", "--depth", "1", "-q"}
	if ref != "" {
		args = append(args, "--branch", ref)
	}
	args = append(args, url, dest)
	cmd := exec.CommandContext(ctx, "git", args...)
	if err := run(cmd); err != nil {
		return fmt.Errorf("git clone %s: %w", redact(url), err)
	}
	return nil
}

func normalizeGitURL(url string) string {
	if strings.HasPrefix(url, "github.com/") {
		u := url
		if !strings.HasSuffix(u, ".git") {
			u += ".git"
		}
		return "https://" + u
	}
	return url
}

func splitRef(s string) (url, ref string) {
	idx := strings.LastIndex(s, "@")
	if idx < 0 {
		return s, ""
	}
	lastSlash := strings.LastIndex(s, "/")
	if lastSlash < idx {
		return s[:idx], s[idx+1:]
	}
	return s, ""
}

func gitHead(ctx context.Context, dir string) (string, error) {
	cmd := exec.CommandContext(ctx, "git", "-C", dir, "rev-parse", "--short", "HEAD")
	out, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(out)), nil
}

func redact(url string) string {
	if i := strings.Index(url, "://"); i >= 0 {
		rest := url[i+3:]
		if at := strings.Index(rest, "@"); at >= 0 {
			return url[:i+3] + "***" + rest[at:]
		}
	}
	return url
}
