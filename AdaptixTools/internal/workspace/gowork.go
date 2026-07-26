package workspace

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"os"
	"strings"
)

const GoWorkFileName = "go.work"

type Patcher struct {
	path  string
	lines []string
}

func New(path string) (*Patcher, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read go.work %s: %w", path, err)
	}
	p := &Patcher{path: path}
	sc := bufio.NewScanner(bytes.NewReader(data))
	sc.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for sc.Scan() {
		p.lines = append(p.lines, sc.Text())
	}
	if err := sc.Err(); err != nil {
		return nil, fmt.Errorf("scan go.work %s: %w", path, err)
	}
	return p, nil
}

func (p *Patcher) Bytes() []byte {
	return []byte(strings.Join(p.lines, "\n") + "\n")
}

func (p *Patcher) Save() error {
	if err := os.WriteFile(p.path, p.Bytes(), 0o644); err != nil {
		return fmt.Errorf("write go.work %s: %w", p.path, err)
	}
	return nil
}

func (p *Patcher) Has(usePath string) bool {
	return p.findEntry(usePath) >= 0
}

func (p *Patcher) findEntry(usePath string) int {
	inBlock := false
	for i, raw := range p.lines {
		trim := strings.TrimSpace(raw)
		if strings.HasPrefix(trim, "use") && strings.HasSuffix(trim, "(") {
			inBlock = true
			continue
		}
		if inBlock {
			if trim == ")" {
				inBlock = false
				continue
			}
			if strings.HasPrefix(trim, "//") {
				continue
			}
			if trim == usePath {
				return i
			}
			continue
		}

		if strings.HasPrefix(trim, "use ") && !strings.HasSuffix(trim, "(") {
			rest := strings.TrimSpace(strings.TrimPrefix(trim, "use"))

			if j := strings.Index(rest, "//"); j >= 0 {
				rest = strings.TrimSpace(rest[:j])
			}
			if rest == usePath {
				return i
			}
		}
	}
	return -1
}

func (p *Patcher) Add(usePath string) error {
	if p.findEntry(usePath) >= 0 {
		return nil
	}
	insertAt, inBlock, err := p.insertPoint()
	if err != nil {
		return err
	}
	var newLine string
	if inBlock {
		indent := "\t"
		for j := insertAt - 1; j >= 0; j-- {
			line := p.lines[j]
			t := strings.TrimSpace(line)
			if t == "" || strings.HasPrefix(t, "//") {
				continue
			}
			if i := strings.IndexAny(line, " \t"); i >= 0 {

				k := 0
				for k < len(line) && (line[k] == ' ' || line[k] == '\t') {
					k++
				}
				if k > 0 {
					indent = line[:k]
				}
			}
			break
		}
		newLine = indent + usePath
	} else {
		newLine = "use " + usePath
	}
	p.lines = append(p.lines[:insertAt], append([]string{newLine}, p.lines[insertAt:]...)...)
	return nil
}

func (p *Patcher) Remove(usePath string, commentMode bool) error {
	idx := p.findEntry(usePath)
	if idx < 0 {
		return nil
	}
	if commentMode {
		line := p.lines[idx]
		k := 0
		for k < len(line) && (line[k] == ' ' || line[k] == '\t') {
			k++
		}
		p.lines[idx] = line[:k] + "//" + strings.TrimLeft(line[k:], " \t")
	} else {
		p.lines = append(p.lines[:idx], p.lines[idx+1:]...)
	}
	return nil
}

func (p *Patcher) insertPoint() (idx int, inBlock bool, err error) {
	for i, raw := range p.lines {
		trim := strings.TrimSpace(raw)
		if strings.HasPrefix(trim, "use") && strings.HasSuffix(trim, "(") {

			for j := i + 1; j < len(p.lines); j++ {
				if strings.TrimSpace(p.lines[j]) == ")" {
					return j, true, nil
				}
			}
			return -1, false, errors.New("go.work: unclosed `use (` block")
		}
	}

	return len(p.lines), false, nil
}
