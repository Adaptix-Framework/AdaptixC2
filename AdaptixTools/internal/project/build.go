package project

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"

	"axtool/internal/build"
	"axtool/internal/fsutil"
	"axtool/internal/ui"
)

type BuildOptions struct {
	Jobs      int
	NoProfile bool
}

func (l Layout) BuildServer(ctx context.Context, out io.Writer, opts BuildOptions) error {
	if out == nil {
		out = io.Discard
	}
	if err := os.MkdirAll(l.DistDir, 0o755); err != nil {
		return err
	}
	bin := filepath.Join(l.ServerDir, "adaptixserver")
	_ = os.Remove(bin)

	prog := ui.NewProgress(out)
	prog.Start(2)
	defer prog.Stop()

	prog.StartStep(1, 2, "go build adaptixserver")
	pw := &ui.ProgressWriter{Progress: prog}
	runner := &build.Runner{Dir: l.ServerDir, Out: out, Progress: pw}
	cmd := `go build -buildvcs=false -ldflags="-s -w" -o adaptixserver .`
	if err := runner.RunOne(ctx, cmd); err != nil {
		prog.Fail("server build failed")
		return fmt.Errorf("server build: %w", err)
	}
	prog.SetPercent(100)

	stageLabel := filepath.Base(l.DistDir)
	if stageLabel == "" || stageLabel == "." {
		stageLabel = l.DistDir
	}
	prog.StartStep(2, 2, "stage to "+stageLabel+"/")
	dst := filepath.Join(l.DistDir, "adaptixserver")
	if err := fsutil.CopyFile(bin, dst); err != nil {
		if err2 := os.Rename(bin, dst); err2 != nil {
			prog.Fail("install binary failed")
			return fmt.Errorf("install binary: %v / %w", err, err2)
		}
	} else {
		_ = os.Remove(bin)
	}
	if err := os.Chmod(dst, 0o755); err != nil {
		prog.Fail(err.Error())
		return err
	}

	for _, name := range []string{"ssl_gen.sh", "404page.html"} {
		src := filepath.Join(l.ServerDir, name)
		dstf := filepath.Join(l.DistDir, name)
		if _, err := os.Stat(src); err != nil {
			continue
		}
		if _, err := os.Stat(dstf); err == nil {
			continue
		}
		if err := fsutil.CopyFile(src, dstf); err != nil {
			prog.Fail(err.Error())
			return fmt.Errorf("copy %s: %w", name, err)
		}
		if name == "ssl_gen.sh" {
			_ = os.Chmod(dstf, 0o755)
		}
	}
	if !opts.NoProfile {

		srcProf := filepath.Join(l.ServerDir, "profile.yaml")
		dstProf := l.Spec.ProfilePath(l.ProjectRoot)
		if _, err := os.Stat(dstProf); err != nil {
			if _, err := os.Stat(srcProf); err == nil {
				if err := os.MkdirAll(filepath.Dir(dstProf), 0o755); err != nil {
					prog.Fail(err.Error())
					return fmt.Errorf("mkdir profile dir: %w", err)
				}
				if err := fsutil.CopyFile(srcProf, dstProf); err != nil {
					prog.Fail(err.Error())
					return fmt.Errorf("copy profile → %s: %w", dstProf, err)
				}
			}
		}
	}
	prog.SetPercent(100)
	prog.Success("server → " + dst)
	return nil
}

func (l Layout) BuildClient(ctx context.Context, out io.Writer, opts BuildOptions) error {
	if out == nil {
		out = io.Discard
	}
	if _, err := os.Stat(l.ClientDir); err != nil {
		return fmt.Errorf("client dir %s: %w", l.ClientDir, err)
	}
	if err := os.MkdirAll(l.DistDir, 0o755); err != nil {
		return err
	}
	jobs := opts.Jobs
	if jobs <= 0 {
		jobs = runtime.NumCPU()
		if jobs < 1 {
			jobs = 1
		}
	}

	prog := ui.NewProgress(out)
	prog.Start(3)
	defer prog.Stop()

	pw := &ui.ProgressWriter{Progress: prog}
	runner := &build.Runner{Dir: l.ClientDir, Out: out, Progress: pw}

	prog.StartStep(1, 3, "cmake .")
	if err := runner.RunOne(ctx, "cmake ."); err != nil {
		prog.Fail("cmake failed")
		return fmt.Errorf("cmake: %w", err)
	}
	prog.SetPercent(100)

	prog.StartStep(2, 3, "compile")
	buildCmd := "cmake --build . --parallel " + strconv.Itoa(jobs)
	if stdbuf, err := exec.LookPath("stdbuf"); err == nil {
		buildCmd = stdbuf + " -oL -eL " + buildCmd
	}
	if err := runner.RunOne(ctx, buildCmd); err != nil {
		prog.Fail("compile failed")
		return fmt.Errorf("cmake --build: %w", err)
	}
	prog.SetPercent(100)

	prog.StartStep(3, 3, "stage to "+filepath.Base(l.DistDir)+"/")

	srcBin := filepath.Join(l.ClientDir, "AdaptixClient")
	if _, err := os.Stat(srcBin); err != nil {
		alt := filepath.Join(l.ClientDir, "build", "AdaptixClient")
		if _, err2 := os.Stat(alt); err2 == nil {
			srcBin = alt
		} else {
			prog.Fail("client binary not found")
			return fmt.Errorf("client binary not found at %s (or %s); client_dir from adaptix.spec is %s",
				filepath.Join(l.ClientDir, "AdaptixClient"), alt, l.ClientDir)
		}
	}

	if err := os.MkdirAll(l.DistDir, 0o755); err != nil {
		prog.Fail(err.Error())
		return err
	}
	dst := filepath.Join(l.DistDir, "AdaptixClient")
	if err := fsutil.CopyFile(srcBin, dst); err != nil {
		if err2 := os.Rename(srcBin, dst); err2 != nil {
			prog.Fail("install client binary failed")
			return fmt.Errorf("install client binary → %s: %v / %w", dst, err, err2)
		}
	}
	_ = os.Chmod(dst, 0o755)
	prog.SetPercent(100)
	prog.Success("client → " + dst)
	return nil
}

func Which(name string) (string, error) {
	return exec.LookPath(name)
}
