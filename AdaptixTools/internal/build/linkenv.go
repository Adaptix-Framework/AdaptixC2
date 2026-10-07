package build

import (
	"os/exec"
	"runtime"
	"strings"
)

var (
	goOS    = runtime.GOOS
	goArch  = runtime.GOARCH
	hasGold = lookupGold
)

func lookupGold() bool {
	_, err := exec.LookPath("ld.gold")
	return err == nil
}

func needsBfdExtld() bool {
	if goOS != "linux" || goArch != "arm64" {
		return false
	}
	return !hasGold()
}

func GoLdflags(base string) string {
	if !needsBfdExtld() {
		return base
	}
	if base == "" {
		return "-extldflags=-fuse-ld=bfd"
	}
	return base + " -extldflags=-fuse-ld=bfd"
}

func extraGoLinkEnvFrom(env []string) []string {
	if !needsBfdExtld() {
		return nil
	}
	var out []string
	if envValue(env, "GO_LDFLAGS") == "" {
		out = append(out, "GO_LDFLAGS="+GoLdflags("-s -w"))
	}
	val := envValue(env, "CGO_LDFLAGS")
	if !strings.Contains(val, "-fuse-ld=") {
		if val == "" {
			val = "-fuse-ld=bfd"
		} else {
			val = val + " -fuse-ld=bfd"
		}
		out = append(out, "CGO_LDFLAGS="+val)
	}
	return out
}

func envValue(env []string, key string) string {
	prefix := key + "="
	for i := len(env) - 1; i >= 0; i-- {
		if strings.HasPrefix(env[i], prefix) {
			return env[i][len(prefix):]
		}
	}
	return ""
}
