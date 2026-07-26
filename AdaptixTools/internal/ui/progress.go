package ui

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

type Progress struct {
	w       io.Writer
	tty     bool
	mu      sync.Mutex
	active  bool
	step    int
	steps   int
	pct     int
	base    string
	detail  string
	frame   int
	stopCh  chan struct{}
	stopped bool
	lastOut int
}

var makePctRE = regexp.MustCompile(`\[\s*(\d{1,3})\s*%\s*\]`)

var ninjaFracRE = regexp.MustCompile(`\[\s*(\d+)\s*/\s*(\d+)\s*\]`)

var buildingObjRE = regexp.MustCompile(`(?i)Building\s+\S+\s+object\s+(\S+)`)

var linkingRE = regexp.MustCompile(`(?i)Linking\s+\S+\s+(?:executable|shared library|static library)\s+(\S+)`)
var builtTargetRE = regexp.MustCompile(`(?i)Built target\s+(\S+)`)

var ansiRE = regexp.MustCompile(`\x1b\[[0-9;]*[A-Za-z]`)

func NewProgress(w io.Writer) *Progress {
	if w == nil {
		w = io.Discard
	}
	raw := w
	if cw, ok := w.(*ColorWriter); ok {
		raw = cw.W
	}
	return &Progress{
		w:       w,
		tty:     colorEnabled(raw) || forceProgress(),
		pct:     -1,
		lastOut: -1,
	}
}

func forceProgress() bool {
	return os.Getenv("FORCE_COLOR") != "" || os.Getenv("AXTOOL_PROGRESS") == "1"
}

func (p *Progress) Start(steps int) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.steps = steps
	p.step = 0
	p.stopped = false
	p.lastOut = -1
	if p.tty && p.stopCh == nil {
		p.stopCh = make(chan struct{})
		go p.spinLoop()
	}
}

func (p *Progress) StartStep(step, steps int, label string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.steps = steps
	p.step = step
	p.base = label
	p.detail = ""
	p.pct = -1
	p.lastOut = -1
	p.active = true
	if !p.tty {
		fmt.Fprintf(p.w, "[build] (%d/%d) %s\n", step, steps, label)
		return
	}
	p.renderLocked()
}

func (p *Progress) SetPercent(pct int) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.setPercentLocked(pct, false)
}

func (p *Progress) setPercentLocked(pct int, force bool) {
	if pct < 0 {
		pct = 0
	}
	if pct > 100 {
		pct = 100
	}
	if !force && p.pct >= 0 && pct < p.pct && pct != 0 {

		pct = p.pct
	}
	p.pct = pct
	p.active = true
	if p.tty {
		p.renderLocked()
		return
	}

	if pct > p.lastOut {
		p.lastOut = pct
		fmt.Fprintf(p.w, "[build] (%d/%d) %3d%% %s\n", p.step, p.steps, pct, p.displayLabel())
	}
}

func (p *Progress) ParseLine(line string) {
	line = stripANSI(line)
	line = strings.TrimSpace(line)
	if line == "" {
		return
	}

	pct, hasPct := parseBuildPercent(line)
	detail := extractBuildDetail(line)

	p.mu.Lock()
	defer p.mu.Unlock()
	if detail != "" {
		p.detail = detail
	}
	if hasPct {
		p.setPercentLocked(pct, false)
		return
	}

	if detail != "" && p.tty && p.active {
		p.renderLocked()
	}
}

func (p *Progress) Suspend() {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.active = false
	if p.tty {
		fmt.Fprint(p.w, "\r\033[2K")
	}
}

func (p *Progress) Success(msg string) {
	p.finish(true, msg)
}

func (p *Progress) Fail(msg string) {
	p.finish(false, msg)
}

func (p *Progress) Stop() {
	p.finish(true, "")
}

func (p *Progress) finish(ok bool, msg string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.stopped {
		return
	}
	p.stopped = true
	p.active = false
	if p.stopCh != nil {
		close(p.stopCh)
		p.stopCh = nil
	}
	if p.tty {
		fmt.Fprint(p.w, "\r\033[2K")
	}
	if msg == "" {
		return
	}
	if ok {
		fmt.Fprintf(p.w, "[ok] %s\n", msg)
	} else {
		fmt.Fprintf(p.w, "[error] %s\n", msg)
	}
}

func (p *Progress) spinLoop() {
	t := time.NewTicker(80 * time.Millisecond)
	defer t.Stop()
	for {
		select {
		case <-p.stopCh:
			return
		case <-t.C:
			p.mu.Lock()
			if p.active && !p.stopped && p.tty {
				p.frame++

				if p.pct < 0 {
					p.renderLocked()
				}
			}
			p.mu.Unlock()
		}
	}
}

var spinnerFrames = []string{"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"}

func (p *Progress) displayLabel() string {
	if p.detail == "" {
		return p.base
	}
	if p.base == "" {
		return p.detail
	}
	return p.base + " · " + p.detail
}

func (p *Progress) renderLocked() {
	width := 28
	var bar string
	var right string
	if p.pct >= 0 {
		filled := p.pct * width / 100
		if filled > width {
			filled = width
		}
		bar = strings.Repeat("#", filled) + strings.Repeat("-", width-filled)
		right = fmt.Sprintf("%3d%%", p.pct)
	} else {
		pos := p.frame % (width * 2)
		if pos >= width {
			pos = width*2 - pos - 1
		}
		if pos < 0 {
			pos = 0
		}
		b := []byte(strings.Repeat("-", width))
		for i := 0; i < 4 && pos+i < width; i++ {
			b[pos+i] = '#'
		}
		bar = string(b)
		spin := spinnerFrames[p.frame%len(spinnerFrames)]
		right = spin + "   "
	}
	step := ""
	if p.steps > 0 && p.step > 0 {
		step = fmt.Sprintf("(%d/%d) ", p.step, p.steps)
	}
	label := p.displayLabel()
	line := fmt.Sprintf("\r\033[2K[build] %s[%s] %s %s", step, bar, right, label)
	if len(line) > 140 {
		line = line[:137] + "..."
	}
	fmt.Fprint(p.w, line)
}

type ProgressWriter struct {
	Progress *Progress
	buf      []byte
}

func (w *ProgressWriter) IsProgressSink() bool { return true }

func (w *ProgressWriter) Write(p []byte) (int, error) {
	if w.Progress == nil {
		return len(p), nil
	}
	w.buf = append(w.buf, p...)
	for {
		i := bytesIndexByte(w.buf, '\n')
		j := bytesIndexByte(w.buf, '\r')
		cut := -1
		switch {
		case i >= 0 && j >= 0:
			if i < j {
				cut = i
			} else {
				cut = j
			}
		case i >= 0:
			cut = i
		case j >= 0:
			cut = j
		}
		if cut < 0 {

			if makePctRE.Match(w.buf) || ninjaFracRE.Match(w.buf) ||
				bytesContainsFold(w.buf, []byte("Building")) {
				w.Progress.ParseLine(string(w.buf))
			}
			break
		}
		line := string(w.buf[:cut])
		w.buf = w.buf[cut+1:]
		if line == "" {
			continue
		}
		w.Progress.ParseLine(line)
	}
	return len(p), nil
}

func bytesIndexByte(b []byte, c byte) int {
	for i := 0; i < len(b); i++ {
		if b[i] == c {
			return i
		}
	}
	return -1
}

func bytesContainsFold(b, sub []byte) bool {
	return strings.Contains(strings.ToLower(string(b)), strings.ToLower(string(sub)))
}

func stripANSI(s string) string {
	return ansiRE.ReplaceAllString(s, "")
}

func parseBuildPercent(line string) (int, bool) {
	line = stripANSI(line)
	if m := makePctRE.FindStringSubmatch(line); len(m) >= 2 {
		var n int
		if _, err := fmt.Sscanf(m[1], "%d", &n); err == nil && n >= 0 && n <= 100 {
			return n, true
		}
	}
	if m := ninjaFracRE.FindStringSubmatch(line); len(m) >= 3 {
		var cur, total int
		if _, err := fmt.Sscanf(m[1], "%d", &cur); err != nil {
			return 0, false
		}
		if _, err := fmt.Sscanf(m[2], "%d", &total); err != nil || total <= 0 {
			return 0, false
		}
		n := cur * 100 / total
		if n > 100 {
			n = 100
		}
		if n < 0 {
			n = 0
		}
		return n, true
	}
	return 0, false
}

func extractBuildDetail(line string) string {
	line = stripANSI(line)
	if m := buildingObjRE.FindStringSubmatch(line); len(m) >= 2 {
		return filepath.Base(m[1])
	}
	if m := linkingRE.FindStringSubmatch(line); len(m) >= 2 {
		return "link " + filepath.Base(m[1])
	}
	if m := builtTargetRE.FindStringSubmatch(line); len(m) >= 2 {
		return "done " + m[1]
	}
	return ""
}
