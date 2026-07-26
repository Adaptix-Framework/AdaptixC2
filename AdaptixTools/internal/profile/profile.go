package profile

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

const ProfileFileName = "profile.yaml"

const (
	ListExtenders = "extenders"
	ListAxScripts = "axscripts"
)

type Patcher struct {
	path  string
	lines []string
}

func New(path string) (*Patcher, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read profile %s: %w", path, err)
	}
	p := &Patcher{path: path}
	sc := bufio.NewScanner(bytes.NewReader(data))
	sc.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for sc.Scan() {
		p.lines = append(p.lines, sc.Text())
	}
	if err := sc.Err(); err != nil {
		return nil, fmt.Errorf("scan profile %s: %w", path, err)
	}
	return p, nil
}

func (p *Patcher) Bytes() []byte {
	return []byte(strings.Join(p.lines, "\n") + "\n")
}

func (p *Patcher) Save() error {
	if err := os.WriteFile(p.path, p.Bytes(), 0o644); err != nil {
		return fmt.Errorf("write profile %s: %w", p.path, err)
	}
	return nil
}

func (p *Patcher) Has(path string) bool {
	return p.HasIn(ListExtenders, path)
}

func (p *Patcher) Add(path string) error {
	return p.AddTo(ListExtenders, path)
}

func (p *Patcher) Remove(path string) error {
	return p.RemoveFrom(ListExtenders, path)
}

func (p *Patcher) HasIn(list, path string) bool {
	_, ok := p.findEntry(list, path)
	return ok
}

func (p *Patcher) AddTo(list, path string) error {
	start := p.findListHeader(list)
	if start < 0 {
		if err := p.ensureList(list); err != nil {
			return err
		}
		start = p.findListHeader(list)
		if start < 0 {
			return fmt.Errorf("profile.yaml: no %q list under Teamserver:", list)
		}
	}
	if _, ok := p.findEntry(list, path); ok {
		return nil
	}
	insertAt := p.listEndIndex(start)
	indent := "    "
	for j := insertAt - 1; j > start; j-- {
		line := p.lines[j]
		t := strings.TrimSpace(line)
		if strings.HasPrefix(t, "-") {
			k := 0
			for k < len(line) && (line[k] == ' ' || line[k] == '\t') {
				k++
			}
			if k > 0 {
				indent = line[:k]
			}
			break
		}
	}
	newLine := indent + "- \"" + path + "\""
	p.lines = append(p.lines[:insertAt], append([]string{newLine}, p.lines[insertAt:]...)...)
	return nil
}

func (p *Patcher) RemoveFrom(list, path string) error {
	idx, ok := p.findEntry(list, path)
	if !ok {
		return nil
	}
	p.lines = append(p.lines[:idx], p.lines[idx+1:]...)
	return nil
}

func (p *Patcher) ListIn(list string) []string {
	start := p.findListHeader(list)
	if start < 0 {
		return nil
	}
	var out []string
	for i := start + 1; i < len(p.lines); i++ {
		raw := p.lines[i]
		t := strings.TrimSpace(raw)
		if t == "" {
			break
		}
		if strings.HasPrefix(raw, "  ") && !strings.HasPrefix(raw, "    ") &&
			!strings.HasPrefix(t, "-") && !strings.HasPrefix(t, "#") {
			break
		}
		if strings.HasPrefix(t, "#") {
			continue
		}
		if !strings.HasPrefix(t, "-") {
			continue
		}
		if item := extractListItem(t); item != "" {
			out = append(out, item)
		}
	}
	return out
}

func (p *Patcher) PruneMissing(list, baseDir string) ([]string, error) {
	var removed []string
	for _, entry := range p.ListIn(list) {
		check := entry
		if !filepath.IsAbs(check) {
			check = filepath.Join(baseDir, entry)
		}
		if _, err := os.Stat(check); err == nil {
			continue
		}
		if err := p.RemoveFrom(list, entry); err != nil {
			return removed, err
		}
		removed = append(removed, entry)
	}
	return removed, nil
}

func (p *Patcher) findListHeader(list string) int {
	want := "  " + list + ":"
	inTeamserver := false
	for i, raw := range p.lines {
		t := strings.TrimRight(raw, " \t")
		if raw != "" && raw[0] != ' ' && raw[0] != '\t' && raw[0] != '#' {
			inTeamserver = strings.HasPrefix(t, "Teamserver:")
			continue
		}
		if !inTeamserver {
			continue
		}
		if strings.HasPrefix(raw, want) {
			return i
		}
	}
	return -1
}

func (p *Patcher) ensureList(list string) error {
	teamIdx := -1
	for i, raw := range p.lines {
		t := strings.TrimRight(raw, " \t")
		if raw != "" && raw[0] != ' ' && raw[0] != '\t' && raw[0] != '#' && strings.HasPrefix(t, "Teamserver:") {
			teamIdx = i
			break
		}
	}
	if teamIdx < 0 {
		return errors.New("profile.yaml: no Teamserver: section")
	}
	insertAt := teamIdx + 1
	if ext := p.findListHeader(ListExtenders); ext >= 0 {
		insertAt = p.listEndIndex(ext)
	}
	header := "  " + list + ":"
	p.lines = append(p.lines[:insertAt], append([]string{header}, p.lines[insertAt:]...)...)
	return nil
}

func (p *Patcher) findEntry(list, path string) (int, bool) {
	start := p.findListHeader(list)
	if start < 0 {
		return -1, false
	}
	for i := start + 1; i < len(p.lines); i++ {
		raw := p.lines[i]
		t := strings.TrimSpace(raw)
		if t == "" {
			return -1, false
		}
		if strings.HasPrefix(raw, "  ") && !strings.HasPrefix(raw, "    ") &&
			!strings.HasPrefix(t, "-") && !strings.HasPrefix(t, "#") {
			return -1, false
		}
		if strings.HasPrefix(t, "#") {
			continue
		}
		if !strings.HasPrefix(t, "-") {
			continue
		}
		if extractListItem(t) == path {
			return i, true
		}
	}
	return -1, false
}

func extractListItem(item string) string {
	s := strings.TrimSpace(item)
	s = strings.TrimPrefix(s, "-")
	s = strings.TrimSpace(s)
	s = strings.Trim(s, "\"'")
	return s
}

func (p *Patcher) listEndIndex(start int) int {
	for i := start + 1; i < len(p.lines); i++ {
		raw := p.lines[i]
		t := strings.TrimSpace(raw)
		if t == "" {
			return i
		}
		if strings.HasPrefix(raw, "  ") && !strings.HasPrefix(raw, "    ") &&
			!strings.HasPrefix(t, "-") && !strings.HasPrefix(t, "#") {
			return i
		}
	}
	return len(p.lines)
}

func (p *Patcher) Path() string { return p.path }

var TeamserverScalars = []string{
	"interface",
	"port",
	"endpoint",
	"password",
	"manage_password",
	"only_password",
	"cert",
	"key",
	"access_token_live_hours",
	"refresh_token_live_hours",
}

func (p *Patcher) GetTeamserver(key string) (string, bool) {
	idx := p.findTeamserverKey(key)
	if idx < 0 {
		return "", false
	}
	return parseYAMLScalarValue(p.lines[idx]), true
}

func (p *Patcher) SetTeamserver(key, value string) error {
	if key == "" {
		return errors.New("empty key")
	}
	formatted := formatYAMLScalar(value)
	line := "  " + key + ": " + formatted
	if idx := p.findTeamserverKey(key); idx >= 0 {
		p.lines[idx] = line
		return nil
	}
	teamIdx := p.findTeamserverHeader()
	if teamIdx < 0 {
		return errors.New("profile.yaml: no Teamserver: section")
	}
	insertAt := teamIdx + 1
	if ext := p.findListHeader(ListExtenders); ext >= 0 {
		insertAt = ext
	} else if ax := p.findListHeader(ListAxScripts); ax >= 0 {
		insertAt = ax
	} else {
		for i := teamIdx + 1; i < len(p.lines); i++ {
			raw := p.lines[i]
			t := strings.TrimSpace(raw)
			if t == "" || strings.HasPrefix(t, "#") {
				continue
			}
			if !strings.HasPrefix(raw, "  ") {
				break
			}
			if strings.HasPrefix(raw, "    ") {
				continue
			}
			insertAt = i + 1
		}
	}
	p.lines = append(p.lines[:insertAt], append([]string{line}, p.lines[insertAt:]...)...)
	return nil
}

func (p *Patcher) ListTeamserverScalars() [][2]string {
	var out [][2]string
	for _, k := range TeamserverScalars {
		if v, ok := p.GetTeamserver(k); ok {
			out = append(out, [2]string{k, v})
		}
	}
	return out
}

func (p *Patcher) findTeamserverHeader() int {
	for i, raw := range p.lines {
		t := strings.TrimRight(raw, " \t")
		if raw != "" && raw[0] != ' ' && raw[0] != '\t' && raw[0] != '#' && strings.HasPrefix(t, "Teamserver:") {
			return i
		}
	}
	return -1
}

func (p *Patcher) findTeamserverKey(key string) int {
	want := "  " + key + ":"
	in := false
	for i, raw := range p.lines {
		t := strings.TrimRight(raw, " \t")
		if raw != "" && raw[0] != ' ' && raw[0] != '\t' && raw[0] != '#' {
			in = strings.HasPrefix(t, "Teamserver:")
			continue
		}
		if !in {
			continue
		}
		if raw != "" && raw[0] != ' ' && raw[0] != '\t' && raw[0] != '#' {
			return -1
		}
		trim := strings.TrimSpace(raw)
		if strings.HasPrefix(trim, "#") {
			continue
		}
		if strings.HasPrefix(raw, want) || strings.HasPrefix(trim, key+":") && strings.HasPrefix(raw, "  ") {
			rest := strings.TrimPrefix(raw, "  ")
			if strings.HasPrefix(rest, key+":") {
				return i
			}
		}
	}
	return -1
}

func parseYAMLScalarValue(line string) string {
	idx := strings.Index(line, ":")
	if idx < 0 {
		return ""
	}
	v := strings.TrimSpace(line[idx+1:])
	if i := strings.Index(v, " #"); i >= 0 {
		v = strings.TrimSpace(v[:i])
	}
	if len(v) >= 2 {
		if (v[0] == '"' && v[len(v)-1] == '"') || (v[0] == '\'' && v[len(v)-1] == '\'') {
			return v[1 : len(v)-1]
		}
	}
	return v
}

func formatYAMLScalar(v string) string {
	v = strings.TrimSpace(v)
	if v == "" {
		return `""`
	}
	if v == "true" || v == "false" {
		return v
	}
	allDigit := true
	for i, r := range v {
		if r >= '0' && r <= '9' {
			continue
		}
		if r == '-' && i == 0 {
			continue
		}
		allDigit = false
		break
	}
	if allDigit {
		return v
	}
	esc := strings.ReplaceAll(v, `\`, `\\`)
	esc = strings.ReplaceAll(esc, `"`, `\"`)
	return `"` + esc + `"`
}
