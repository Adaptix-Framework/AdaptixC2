package build

import "testing"

func TestGoLdflagsNonLinuxArm64(t *testing.T) {
	origOS, origArch, origGold := goOS, goArch, hasGold
	t.Cleanup(func() {
		goOS, goArch, hasGold = origOS, origArch, origGold
	})

	goOS, goArch, hasGold = "linux", "amd64", func() bool { return false }
	if got := GoLdflags("-s -w"); got != "-s -w" {
		t.Fatalf("linux/amd64: got %q", got)
	}

	goOS, goArch, hasGold = "darwin", "arm64", func() bool { return false }
	if got := GoLdflags("-s -w"); got != "-s -w" {
		t.Fatalf("darwin/arm64: got %q", got)
	}
}

func TestGoLdflagsLinuxArm64(t *testing.T) {
	origOS, origArch, origGold := goOS, goArch, hasGold
	t.Cleanup(func() {
		goOS, goArch, hasGold = origOS, origArch, origGold
	})

	goOS, goArch = "linux", "arm64"

	hasGold = func() bool { return true }
	if got := GoLdflags("-s -w"); got != "-s -w" {
		t.Fatalf("with gold: got %q", got)
	}

	hasGold = func() bool { return false }
	if got := GoLdflags("-s -w"); got != "-s -w -extldflags=-fuse-ld=bfd" {
		t.Fatalf("without gold: got %q", got)
	}
	if got := GoLdflags(""); got != "-extldflags=-fuse-ld=bfd" {
		t.Fatalf("empty base: got %q", got)
	}
}

func TestExtraGoLinkEnvFrom(t *testing.T) {
	origOS, origArch, origGold := goOS, goArch, hasGold
	t.Cleanup(func() {
		goOS, goArch, hasGold = origOS, origArch, origGold
	})

	goOS, goArch, hasGold = "linux", "arm64", func() bool { return false }

	got := extraGoLinkEnvFrom(nil)
	if len(got) != 1 || got[0] != "CGO_LDFLAGS=-fuse-ld=bfd" {
		t.Fatalf("empty env: got %q", got)
	}

	got = extraGoLinkEnvFrom([]string{"CGO_LDFLAGS=-O2 -g"})
	if len(got) != 1 || got[0] != "CGO_LDFLAGS=-O2 -g -fuse-ld=bfd" {
		t.Fatalf("append: got %q", got)
	}

	got = extraGoLinkEnvFrom([]string{"CGO_LDFLAGS=-fuse-ld=gold"})
	if got != nil {
		t.Fatalf("existing -fuse-ld: got %q", got)
	}

	goOS, goArch = "linux", "amd64"
	if got := extraGoLinkEnvFrom(nil); got != nil {
		t.Fatalf("linux/amd64: got %q", got)
	}
}
