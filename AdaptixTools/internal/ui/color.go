package ui

import (
	"bytes"
	"io"
	"os"
	"strings"
)

type ColorWriter struct {
	W       io.Writer
	enabled bool
	buf     []byte
}

func NewColorWriter(w io.Writer) *ColorWriter {
	return &ColorWriter{W: w, enabled: colorEnabled(w)}
}

func colorEnabled(w io.Writer) bool {
	if os.Getenv("NO_COLOR") != "" {
		return false
	}
	if os.Getenv("FORCE_COLOR") != "" {
		return true
	}
	f, ok := w.(*os.File)
	if !ok {
		return false
	}
	fi, err := f.Stat()
	if err != nil {
		return false
	}
	return (fi.Mode() & os.ModeCharDevice) != 0
}

func (c *ColorWriter) Write(p []byte) (int, error) {
	if !c.enabled {
		return c.W.Write(p)
	}

	if bytes.IndexByte(p, '\r') >= 0 {
		if len(c.buf) > 0 {
			if _, err := c.W.Write(c.buf); err != nil {
				return 0, err
			}
			c.buf = nil
		}
		return c.W.Write(p)
	}
	c.buf = append(c.buf, p...)
	written := len(p)
	for {
		i := bytes.IndexByte(c.buf, '\n')
		if i < 0 {
			break
		}
		line := string(c.buf[:i+1])
		c.buf = c.buf[i+1:]
		if _, err := io.WriteString(c.W, paintLine(line)); err != nil {
			return written, err
		}
	}
	return written, nil
}

func (c *ColorWriter) Flush() error {
	if len(c.buf) == 0 {
		return nil
	}
	line := string(c.buf)
	c.buf = nil
	if c.enabled {
		line = paintLine(line)
	}
	_, err := io.WriteString(c.W, line)
	return err
}

const (
	reset   = "\033[0m"
	bold    = "\033[1m"
	dim     = "\033[2m"
	red     = "\033[31m"
	green   = "\033[32m"
	yellow  = "\033[33m"
	blue    = "\033[34m"
	magenta = "\033[35m"
	cyan    = "\033[36m"
)

func paintLine(line string) string {

	nl := ""
	body := line
	if strings.HasSuffix(body, "\n") {
		nl = "\n"
		body = body[:len(body)-1]
	}

	style := ""
	switch {
	case strings.HasPrefix(body, "[ok]"):
		style = bold + green
	case strings.HasPrefix(body, "[error]"):
		style = bold + red
	case strings.HasPrefix(body, "[warn]"):
		style = bold + yellow
	case strings.HasPrefix(body, "[rollback]"):
		style = bold + magenta
	case strings.HasPrefix(body, "[install]"),
		strings.HasPrefix(body, "[build]"),
		strings.HasPrefix(body, "[source]"),
		strings.HasPrefix(body, "[plan]"):
		style = bold + cyan
	case strings.HasPrefix(body, "  →"), strings.HasPrefix(body, "  ·"):
		style = blue
	case strings.HasPrefix(body, "  ←"):
		style = dim
	case strings.HasPrefix(body, "error:"):
		style = bold + red
	default:
		return body + nl
	}
	return style + body + reset + nl
}
