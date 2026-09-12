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

// needsBfdExtld reports whether Go's linux/arm64 external linker would inject
// -fuse-ld=gold while gold is not installed. gcc then fails with
// "cannot find 'ld'" (https://go.dev/issue/22040).
func needsBfdExtld() bool {
	if goOS != "linux" || goArch != "arm64" {
		return false
	}
	return !hasGold()
}

// GoLdflags appends -extldflags=-fuse-ld=bfd on linux/arm64 hosts that have no
// gold linker, so gcc does not fail looking for ld.gold.
func GoLdflags(base string) string {
	if !needsBfdExtld() {
		return base
	}
	if base == "" {
		return "-extldflags=-fuse-ld=bfd"
	}
	return base + " -extldflags=-fuse-ld=bfd"
}

// extraGoLinkEnvFrom returns CGO_LDFLAGS entries that force bfd when gold is
// missing. env is the already-merged process environment. Existing -fuse-ld=
// values are left alone.
func extraGoLinkEnvFrom(env []string) []string {
	if !needsBfdExtld() {
		return nil
	}
	val := envValue(env, "CGO_LDFLAGS")
	if strings.Contains(val, "-fuse-ld=") {
		return nil
	}
	if val == "" {
		val = "-fuse-ld=bfd"
	} else {
		val = val + " -fuse-ld=bfd"
	}
	return []string{"CGO_LDFLAGS=" + val}
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
