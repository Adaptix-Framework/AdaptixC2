package project

import (
	"bytes"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
)

const DefaultMinQt = "6.9.0"

func DetectQtVersion() (version string, via string, err error) {
	for _, bin := range []string{"qmake6", "qmake"} {
		path, lookErr := exec.LookPath(bin)
		if lookErr != nil {
			continue
		}
		out, runErr := exec.Command(path, "-query", "QT_VERSION").Output()
		if runErr != nil {
			continue
		}
		v := strings.TrimSpace(string(out))
		if v != "" && v != "**Unknown**" {
			return v, path + " -query QT_VERSION", nil
		}
	}

	if path, lookErr := exec.LookPath("pkg-config"); lookErr == nil {
		for _, mod := range []string{"Qt6Core", "Qt6Core.pc"} {
			out, runErr := exec.Command(path, "--modversion", mod).Output()
			if runErr != nil {
				continue
			}
			v := strings.TrimSpace(string(out))
			if v != "" {
				return v, "pkg-config --modversion " + mod, nil
			}
		}
	}

	return "", "", fmt.Errorf("qmake6/qmake/pkg-config could not report Qt6 version (is Qt6 installed and on PATH?)")
}

const qtUpgradeHint = "" +
	"AdaptixClient needs Qt " + DefaultMinQt + "+\n" +
	"  Options:\n" +
	"  1) Use a newer OS / install Qt " + DefaultMinQt + "+ (e.g. official Qt online installer, or a distro with recent Qt6)\n" +
	"  2) Build a standalone client binary via Docker (no host Qt required):\n" +
	"       make docker-build-client\n" +
	"     (from the monorepo root; produces a client AppImage/standalone artifact)"

func RequireQtVersion(min string) (found string, via string, err error) {
	if min == "" {
		min = DefaultMinQt
	}
	found, via, err = DetectQtVersion()
	if err != nil {
		return "", "", fmt.Errorf("Qt %s+ required: %w\n\n%s", min, err, qtUpgradeHint)
	}
	cmp, cmpErr := compareSemver(found, min)
	if cmpErr != nil {
		return found, via, fmt.Errorf("cannot parse Qt version %q: %w", found, cmpErr)
	}
	if cmp < 0 {
		return found, via, fmt.Errorf(
			"Qt %s found (%s), need Qt %s+\n\n%s",
			found, via, min, qtUpgradeHint)
	}
	return found, via, nil
}

func compareSemver(a, b string) (int, error) {
	pa, err := parseSemver(a)
	if err != nil {
		return 0, err
	}
	pb, err := parseSemver(b)
	if err != nil {
		return 0, err
	}
	for i := 0; i < 3; i++ {
		if pa[i] < pb[i] {
			return -1, nil
		}
		if pa[i] > pb[i] {
			return 1, nil
		}
	}
	return 0, nil
}

func parseSemver(s string) ([3]int, error) {
	var out [3]int
	s = strings.TrimSpace(s)
	var b bytes.Buffer
	for _, r := range s {
		if (r >= '0' && r <= '9') || r == '.' {
			b.WriteRune(r)
			continue
		}
		break
	}
	parts := strings.Split(b.String(), ".")
	if len(parts) == 0 || parts[0] == "" {
		return out, fmt.Errorf("empty version")
	}
	for i := 0; i < 3 && i < len(parts); i++ {
		n, err := strconv.Atoi(parts[i])
		if err != nil {
			return out, err
		}
		out[i] = n
	}
	return out, nil
}
