package build

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
)

type ProgressSink interface {
	Write(p []byte) (int, error)
}

type Runner struct {
	Dir string
	Env []string

	Out io.Writer

	Live io.Writer

	Progress ProgressSink

	LogPrefix string
}

func (r *Runner) Run(ctx context.Context, commands []string) error {
	if len(commands) == 0 {
		return errors.New("no build commands")
	}
	for _, cmd := range commands {
		if err := r.RunOne(ctx, cmd); err != nil {
			return err
		}
	}
	return nil
}

func (r *Runner) RunOne(ctx context.Context, command string) error {
	c := exec.CommandContext(ctx, "sh", "-c", command)
	c.Dir = r.Dir
	c.Env = mergeEnv(os.Environ(), r.Env)

	var buf bytes.Buffer
	writers := []io.Writer{&buf}
	if r.Progress != nil {
		writers = append(writers, r.Progress)
	}
	if r.Live != nil {
		writers = append(writers, r.Live)
	}
	sink := io.Writer(&buf)
	if len(writers) == 1 {
		sink = writers[0]
	} else {
		sink = io.MultiWriter(writers...)
	}
	pr, pw := io.Pipe()
	c.Stdout = pw
	c.Stderr = pw
	done := make(chan error, 1)
	go func() {
		done <- c.Run()
		_ = pw.Close()
	}()
	go func() {
		_, _ = io.Copy(sink, pr)
		_ = pr.Close()
	}()
	err := <-done
	if err == nil {
		return nil
	}

	if r.Live == nil {
		r.dumpLog(command, buf.Bytes())
	}
	return fmt.Errorf("build command %q: %w", command, err)
}

func (r *Runner) dumpLog(command string, log []byte) {
	if r.Out == nil {
		return
	}
	pfx := r.LogPrefix
	fmt.Fprintf(r.Out, "%s----- build log: %s -----\n", pfx, command)
	if len(log) == 0 {
		fmt.Fprintf(r.Out, "%s(no output)\n", pfx)
		fmt.Fprintf(r.Out, "%s----- end -----\n", pfx)
		return
	}

	text := string(log)
	if !strings.HasSuffix(text, "\n") {
		text += "\n"
	}
	for _, line := range strings.SplitAfter(text, "\n") {
		if line == "" {
			continue
		}
		fmt.Fprintf(r.Out, "%s%s", pfx, line)
	}
	fmt.Fprintf(r.Out, "%s----- end -----\n", pfx)
}

func mergeEnv(base, overrides []string) []string {
	if len(overrides) == 0 {
		return base
	}
	idx := make(map[string]int, len(base)+len(overrides))
	out := make([]string, 0, len(base)+len(overrides))
	for _, e := range base {
		k := envKey(e)
		idx[k] = len(out)
		out = append(out, e)
	}
	for _, e := range overrides {
		k := envKey(e)
		if i, ok := idx[k]; ok {
			out[i] = e
		} else {
			idx[k] = len(out)
			out = append(out, e)
		}
	}
	return out
}

func envKey(entry string) string {
	for i := 0; i < len(entry); i++ {
		if entry[i] == '=' {
			return entry[:i]
		}
	}
	return entry
}
