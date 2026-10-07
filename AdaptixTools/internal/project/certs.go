package project

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
)

type CertOptions struct {
	Force bool
	Days  int
}

var blandIdentities = []struct {
	C, ST, L, O, CN string
}{
	{"US", "California", "Los Angeles", "Cloud Hosting LLC", "localhost"},
	{"US", "Washington", "Seattle", "Edge Network Inc", "localhost"},
	{"DE", "Bavaria", "Munich", "Net Services GmbH", "webserver"},
	{"NL", "North Holland", "Amsterdam", "Hosted Systems", "localhost"},
	{"GB", "England", "London", "Web Operations Ltd", "localhost.localdomain"},
	{"FR", "Ile-de-France", "Paris", "Reseau Local SAS", "localhost"},
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
		days = 180 + randN(186) // 180–365, not a 10-year C2 default
	}
	id := blandIdentities[randN(len(blandIdentities))]
	subj := fmt.Sprintf("/C=%s/ST=%s/L=%s/O=%s/CN=%s", id.C, id.ST, id.L, id.O, id.CN)
	san := "DNS:localhost,IP:127.0.0.1"
	if id.CN != "localhost" {
		san = "DNS:" + id.CN + ",DNS:localhost,IP:127.0.0.1"
	}

	ext, err := os.CreateTemp(l.DistDir, "cert-ext-*.cnf")
	if err != nil {
		return fmt.Errorf("cert extfile: %w", err)
	}
	extPath := ext.Name()
	defer os.Remove(extPath)
	_, err = fmt.Fprintf(ext, "basicConstraints=CA:FALSE\n"+
		"keyUsage=critical,digitalSignature,keyEncipherment\n"+
		"extendedKeyUsage=serverAuth\n"+
		"subjectAltName=%s\n", san)
	_ = ext.Close()
	if err != nil {
		return fmt.Errorf("write cert extfile: %w", err)
	}

	args := []string{
		"req", "-x509", "-nodes", "-newkey", "rsa:2048", "-sha256",
		"-keyout", key, "-out", crt,
		"-days", strconv.Itoa(days),
		"-subj", subj,
		"-addext", "basicConstraints=CA:FALSE",
		"-addext", "keyUsage=critical,digitalSignature,keyEncipherment",
		"-addext", "extendedKeyUsage=serverAuth",
		"-addext", "subjectAltName=" + san,
	}
	// Prefer -addext (OpenSSL 1.1.1+). Fall back to -extfile if rejected.
	cmd := exec.CommandContext(ctx, "openssl", args...)
	cmd.Dir = l.DistDir
	cmd.Stdout = out
	cmd.Stderr = out
	if err := cmd.Run(); err != nil {
		args = []string{
			"req", "-x509", "-nodes", "-newkey", "rsa:2048", "-sha256",
			"-keyout", key, "-out", crt,
			"-days", strconv.Itoa(days),
			"-subj", subj,
			"-extfile", extPath,
		}
		cmd = exec.CommandContext(ctx, "openssl", args...)
		cmd.Dir = l.DistDir
		cmd.Stdout = out
		cmd.Stderr = out
		if err2 := cmd.Run(); err2 != nil {
			return fmt.Errorf("openssl: %v / fallback: %w", err, err2)
		}
	}
	_ = os.Chmod(key, 0o600)
	_ = os.Chmod(crt, 0o644)
	fmt.Fprintf(out, "[ok] certs → %s (CN=%s, %d days, CA:FALSE)\n", l.DistDir, id.CN, days)
	return nil
}

func randN(n int) int {
	if n <= 0 {
		return 0
	}
	var b [8]byte
	if _, err := rand.Read(b[:]); err != nil {
		return 0
	}
	return int(binary.BigEndian.Uint64(b[:]) % uint64(n))
}
