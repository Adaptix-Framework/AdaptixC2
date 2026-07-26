package server

import (
	"AdaptixServer/core/utils/tformat"
	"bytes"
	"fmt"
	"log"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unicode"
	"unicode/utf8"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/gin-gonic/gin"
)

const (
	logRingCapacity  = 5000
	logBatchMaxItems = 100
	logBatchInterval = 50 * time.Millisecond

	logWriterFlushDelay = 200 * time.Millisecond
	logWriterRatePerSec = 50
	logWriterThrottleEmit = 5 * time.Second
)

type LogManager struct {
	ts *Teamserver

	mu   sync.RWMutex
	ring []adaptix.LogEntry
	head int
	size int

	debug  bool
	nextId atomic.Int64

	pendingMu    sync.Mutex
	pendingBatch []adaptix.LogEntry

	stopCh chan struct{}
}

func NewLogManager(debug bool) *LogManager {
	lm := &LogManager{
		ring:   make([]adaptix.LogEntry, logRingCapacity),
		debug:  debug,
		stopCh: make(chan struct{}),
	}
	go lm.batchLoop()
	return lm
}

func (lm *LogManager) IsDebug() bool { return lm.debug }

func (lm *LogManager) Info(source, category string, level int, format string, args ...any) {
	lm.Add(adaptix.LogEntry{Status: adaptix.LogStatusInfo, Level: level, Source: source, Category: category, Message: fmt.Sprintf(format, args...)})
}

func (lm *LogManager) Success(source, category string, level int, format string, args ...any) {
	lm.Add(adaptix.LogEntry{Status: adaptix.LogStatusSuccess, Level: level, Source: source, Category: category, Message: fmt.Sprintf(format, args...)})
}

func (lm *LogManager) Warn(source, category string, level int, format string, args ...any) {
	lm.Add(adaptix.LogEntry{Status: adaptix.LogStatusWarn, Level: level, Source: source, Category: category, Message: fmt.Sprintf(format, args...)})
}

func (lm *LogManager) Error(source, category string, level int, format string, args ...any) {
	lm.Add(adaptix.LogEntry{Status: adaptix.LogStatusError, Level: level, Source: source, Category: category, Message: fmt.Sprintf(format, args...)})
}

func (lm *LogManager) Debug(source, category string, level int, format string, args ...any) {
	lm.Add(adaptix.LogEntry{Status: adaptix.LogStatusDebug, Level: level, Source: source, Category: category, Message: fmt.Sprintf(format, args...)})
}

func (lm *LogManager) Bind(ts *Teamserver) {
	lm.ts = ts

	gin.DefaultWriter = ts.TsLogWriter(adaptix.LogStatusDebug, "server", "gin")
	gin.DefaultErrorWriter = ts.TsLogWriter(adaptix.LogStatusError, "server", "gin")
	log.SetOutput(ts.TsLogWriter(adaptix.LogStatusDebug, "server", "stdlib"))
	log.SetFlags(0)
}

func (lm *LogManager) Add(entry adaptix.LogEntry) {
	if entry.Status == adaptix.LogStatusDebug && !lm.debug {
		return
	}
	if entry.Id == 0 {
		entry.Id = lm.nextId.Add(1)
	}
	if entry.Time == 0 {
		entry.Time = time.Now().UTC().Unix()
	}
	entry.Source, entry.Category = normalizeLogFields(entry.Source, entry.Category)
	lm.writeRing(entry)
	lm.printStdout(entry)
	lm.queueWS(entry)
}

func normalizeLogFields(source, category string) (string, string) {
	source = strings.TrimSpace(source)
	category = strings.TrimSpace(category)
	if source == "" {
		source = "server"
	}
	if category == "" {
		if i := strings.Index(source, "::"); i > 0 {
			return source[:i], source[i+2:]
		}
		if i := strings.IndexByte(source, ':'); i > 0 {
			return source[:i], source[i+1:]
		}
	}
	return source, category
}

func (lm *LogManager) writeRing(entry adaptix.LogEntry) {
	lm.mu.Lock()
	lm.ring[lm.head] = entry
	lm.head = (lm.head + 1) % len(lm.ring)
	if lm.size < len(lm.ring) {
		lm.size++
	}
	lm.mu.Unlock()
}

func (lm *LogManager) printStdout(entry adaptix.LogEntry) {
	if entry.Status == adaptix.LogStatusDebug && !lm.debug {
		return
	}
	var symbol, color string
	switch entry.Status {
	case adaptix.LogStatusInfo:
		symbol, color = "[*]", tformat.Green
	case adaptix.LogStatusSuccess:
		symbol, color = "[+]", tformat.Blue
	case adaptix.LogStatusWarn:
		symbol, color = "[!]", tformat.Yellow
	case adaptix.LogStatusError:
		symbol, color = "[-]", tformat.Red
	case adaptix.LogStatusDebug:
		symbol, color = "[#]", tformat.Cyan
	default:
		symbol, color = "[?]", tformat.White
	}
	indent := ""
	for i := 0; i < entry.Level; i++ {
		indent += "   "
	}
	timestamp := tformat.SetBold(time.Unix(entry.Time, 0).Format("02/01 15:04:05"))
	mark := tformat.SetColor(symbol, color)
	src := entry.LogSourceKey()
	if src == "" {
		src = "server"
	}
	fmt.Printf("%s%s %s [%s] (%s)\n", indent, mark, entry.Message, timestamp, src)
}

func (lm *LogManager) queueWS(entry adaptix.LogEntry) {
	lm.pendingMu.Lock()
	lm.pendingBatch = append(lm.pendingBatch, entry)
	flush := len(lm.pendingBatch) >= logBatchMaxItems
	lm.pendingMu.Unlock()
	if flush {
		lm.flushBatch()
	}
}

func (lm *LogManager) batchLoop() {
	t := time.NewTicker(logBatchInterval)
	defer t.Stop()
	for {
		select {
		case <-t.C:
			lm.flushBatch()
		case <-lm.stopCh:
			lm.flushBatch()
			return
		}
	}
}

func (lm *LogManager) flushBatch() {
	lm.pendingMu.Lock()
	if len(lm.pendingBatch) == 0 {
		lm.pendingMu.Unlock()
		return
	}
	batch := lm.pendingBatch
	lm.pendingBatch = nil
	lm.pendingMu.Unlock()

	if lm.ts == nil || lm.ts.Broker == nil {
		return
	}
	packet := CreateSpLogBatch(batch)
	lm.ts.TsSyncAllClients(packet)
}

func (lm *LogManager) Stop() {
	close(lm.stopCh)
}

func matchLogEntry(e adaptix.LogEntry, sourceFilter, categoryFilter, contains string) bool {
	if sourceFilter != "" && e.Source != sourceFilter {
		return false
	}
	if categoryFilter != "" && e.Category != categoryFilter {
		return false
	}
	if contains != "" {
		q := strings.ToLower(contains)
		if !strings.Contains(strings.ToLower(e.Source), q) && !strings.Contains(strings.ToLower(e.Category), q) && !strings.Contains(strings.ToLower(e.LogSourceKey()), q) && !strings.Contains(strings.ToLower(e.Message), q) {
			return false
		}
	}
	return true
}

func (lm *LogManager) HasOlderThan(beforeId int64, sourceFilter, categoryFilter, contains string) bool {
	if lm == nil || beforeId <= 0 {
		return false
	}
	lm.mu.RLock()
	defer lm.mu.RUnlock()
	for i := 0; i < lm.size; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		entry := lm.ring[idx]
		if entry.Id <= 0 || entry.Id >= beforeId {
			continue
		}
		if matchLogEntry(entry, sourceFilter, categoryFilter, contains) {
			return true
		}
	}
	return false
}

func (lm *LogManager) Catalog() (sources []string, categories map[string][]string) {
	lm.mu.RLock()
	defer lm.mu.RUnlock()

	srcSet := make(map[string]struct{})
	catSet := make(map[string]map[string]struct{})
	for i := 0; i < lm.size; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		e := lm.ring[idx]
		if e.Source == "" {
			continue
		}
		srcSet[e.Source] = struct{}{}
		if e.Category == "" {
			continue
		}
		if catSet[e.Source] == nil {
			catSet[e.Source] = make(map[string]struct{})
		}
		catSet[e.Source][e.Category] = struct{}{}
	}
	sources = make([]string, 0, len(srcSet))
	for s := range srcSet {
		sources = append(sources, s)
	}
	sort.Strings(sources)
	categories = make(map[string][]string, len(catSet))
	for src, set := range catSet {
		list := make([]string, 0, len(set))
		for c := range set {
			list = append(list, c)
		}
		sort.Strings(list)
		categories[src] = list
	}
	return sources, categories
}

func (lm *LogManager) Page(offset, limit int) ([]adaptix.LogEntry, int) {
	return lm.PageFiltered(offset, limit, "", "", "")
}

func (lm *LogManager) PageFiltered(offset, limit int, sourceFilter, categoryFilter, contains string) ([]adaptix.LogEntry, int) {
	lm.mu.RLock()
	defer lm.mu.RUnlock()

	totalMatch := 0
	for i := 0; i < lm.size; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		if matchLogEntry(lm.ring[idx], sourceFilter, categoryFilter, contains) {
			totalMatch++
		}
	}
	if limit <= 0 || offset < 0 || offset >= totalMatch {
		return []adaptix.LogEntry{}, totalMatch
	}

	out := make([]adaptix.LogEntry, 0, limit)
	matched := 0
	for i := 0; i < lm.size && len(out) < limit; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		entry := lm.ring[idx]
		if !matchLogEntry(entry, sourceFilter, categoryFilter, contains) {
			continue
		}
		if matched < offset {
			matched++
			continue
		}
		out = append(out, entry)
		matched++
	}
	return out, totalMatch
}

func (lm *LogManager) PageBeforeId(beforeId int64, limit int) ([]adaptix.LogEntry, int) {
	return lm.PageBeforeIdFiltered(beforeId, limit, "", "", "")
}

func (lm *LogManager) PageBeforeIdFiltered(beforeId int64, limit int, sourceFilter, categoryFilter, contains string) ([]adaptix.LogEntry, int) {
	lm.mu.RLock()
	defer lm.mu.RUnlock()

	totalMatch := 0
	for i := 0; i < lm.size; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		if matchLogEntry(lm.ring[idx], sourceFilter, categoryFilter, contains) {
			totalMatch++
		}
	}
	if limit <= 0 || totalMatch == 0 {
		return []adaptix.LogEntry{}, totalMatch
	}

	out := make([]adaptix.LogEntry, 0, limit)
	for i := 0; i < lm.size && len(out) < limit; i++ {
		idx := (lm.head - 1 - i + len(lm.ring)) % len(lm.ring)
		entry := lm.ring[idx]
		if !matchLogEntry(entry, sourceFilter, categoryFilter, contains) {
			continue
		}
		if beforeId > 0 && entry.Id >= beforeId {
			continue
		}
		out = append(out, entry)
	}
	return out, totalMatch
}

type LogWriter struct {
	lm       *LogManager
	status   adaptix.LogStatus
	source   string
	category string

	mu      sync.Mutex
	buf     []byte
	pending []string
	timer   *time.Timer

	throttleWindow   time.Time
	throttleCount    int
	throttleDropped  int
	lastThrottleEmit time.Time
}

func newLogWriter(lm *LogManager, status adaptix.LogStatus, source, category string) *LogWriter {
	return &LogWriter{lm: lm, status: status, source: source, category: category}
}

func (w *LogWriter) Write(p []byte) (int, error) {
	if w.lm == nil {
		return len(p), nil
	}
	w.mu.Lock()
	defer w.mu.Unlock()

	w.buf = append(w.buf, p...)
	for {
		i := bytes.IndexByte(w.buf, '\n')
		if i < 0 {
			break
		}
		line := bytes.TrimRight(w.buf[:i], "\r")
		w.buf = w.buf[i+1:]
		if !utf8.Valid(line) {
			line = bytes.ToValidUTF8(line, []byte{'?'})
		}
		if len(line) == 0 {
			w.flushPendingLocked()
			continue
		}
		w.handleLineLocked(string(line))
	}
	w.armTimerLocked()
	return len(p), nil
}

func (w *LogWriter) handleLineLocked(line string) {
	if len(w.pending) > 0 {
		if r, _ := utf8.DecodeRuneInString(line); r != utf8.RuneError && unicode.IsSpace(r) {
			w.pending = append(w.pending, line)
			return
		}
		w.flushPendingLocked()
	}
	w.pending = append(w.pending, line)
}

func (w *LogWriter) flushPendingLocked() {
	if len(w.pending) == 0 {
		return
	}
	msg := strings.Join(w.pending, "\n")
	w.pending = w.pending[:0]

	if !w.throttleAllowLocked() {
		w.throttleDropped++
		return
	}
	w.lm.Add(adaptix.LogEntry{
		Status:   w.status,
		Source:   w.source,
		Category: w.category,
		Message:  msg,
	})
}

func (w *LogWriter) armTimerLocked() {
	if len(w.pending) == 0 && len(w.buf) == 0 && w.throttleDropped == 0 {
		return
	}
	if w.timer == nil {
		w.timer = time.AfterFunc(logWriterFlushDelay, w.onTimer)
		return
	}
	w.timer.Reset(logWriterFlushDelay)
}

func (w *LogWriter) onTimer() {
	w.mu.Lock()
	defer w.mu.Unlock()

	if len(w.buf) > 0 {
		tail := bytes.TrimRight(w.buf, "\r")
		w.buf = w.buf[:0]
		if !utf8.Valid(tail) {
			tail = bytes.ToValidUTF8(tail, []byte{'?'})
		}
		if len(tail) > 0 {
			w.handleLineLocked(string(tail))
		}
	}
	w.flushPendingLocked()
	w.maybeEmitThrottleSummaryLocked()
}

func (w *LogWriter) throttleAllowLocked() bool {
	now := time.Now()
	if now.Sub(w.throttleWindow) >= time.Second {
		w.throttleWindow = now
		w.throttleCount = 0
	}
	if w.throttleCount >= logWriterRatePerSec {
		return false
	}
	w.throttleCount++
	return true
}

func (w *LogWriter) maybeEmitThrottleSummaryLocked() {
	if w.throttleDropped == 0 {
		return
	}
	now := time.Now()
	if !w.lastThrottleEmit.IsZero() && now.Sub(w.lastThrottleEmit) < logWriterThrottleEmit {
		return
	}
	n := w.throttleDropped
	w.throttleDropped = 0
	w.lastThrottleEmit = now
	w.lm.Add(adaptix.LogEntry{
		Status:   adaptix.LogStatusWarn,
		Source:   w.source,
		Category: w.category,
		Message:  fmt.Sprintf("[LogWriter] throttled %d entries (rate cap %d/sec)", n, logWriterRatePerSec),
	})
}
