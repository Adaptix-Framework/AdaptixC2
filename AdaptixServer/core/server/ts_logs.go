package server

import (
	"encoding/json"
	"fmt"
	"io"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsLogAdd(status adaptix.LogStatus, level int, source, category string, format string, args ...any) {
	if ts.LogManager == nil {
		return
	}
	ts.LogManager.Add(adaptix.LogEntry{
		Status:   status,
		Level:    level,
		Source:   source,
		Category: category,
		Message:  fmt.Sprintf(format, args...),
	})
}

func (ts *Teamserver) TsLogWriter(status adaptix.LogStatus, source, category string) io.Writer {
	if ts.LogManager == nil {
		return io.Discard
	}
	return newLogWriter(ts.LogManager, status, source, category)
}

type LogPage struct {
	Items      []adaptix.LogEntry  `json:"items"`
	Total      int                 `json:"total"`
	Offset     int                 `json:"offset"`
	Limit      int                 `json:"limit"`
	HasMore    bool                `json:"has_more"`
	OldestId   int64               `json:"oldest_id,omitempty"`
	Sources    []string            `json:"sources"`
	Categories map[string][]string `json:"categories,omitempty"`
}

func (ts *Teamserver) logCatalogSnapshot() (sources []string, categories map[string][]string) {
	if ts.LogManager == nil {
		return []string{}, map[string][]string{}
	}
	return ts.LogManager.Catalog()
}

func (ts *Teamserver) TsLogsGetPage(offset, limit int) ([]byte, error) {
	return ts.TsLogsGetPageFiltered(offset, limit, "", "", "")
}

func (ts *Teamserver) TsLogsGetPageFiltered(offset, limit int, sourceFilter, categoryFilter, contains string) ([]byte, error) {
	if ts.LogManager == nil {
		return json.Marshal(LogPage{Items: []adaptix.LogEntry{}, Total: 0, Offset: offset, Limit: limit, Sources: []string{}, Categories: map[string][]string{}})
	}
	items, total := ts.LogManager.PageFiltered(offset, limit, sourceFilter, categoryFilter, contains)
	srcs, cats := ts.logCatalogSnapshot()
	var oldest int64
	for _, e := range items {
		if e.Id > 0 && (oldest == 0 || e.Id < oldest) {
			oldest = e.Id
		}
	}
	hasMore := offset+len(items) < total
	if oldest > 0 {
		hasMore = ts.LogManager.HasOlderThan(oldest, sourceFilter, categoryFilter, contains)
	} else if len(items) == 0 {
		hasMore = false
	}
	return json.Marshal(LogPage{
		Items:      items,
		Total:      total,
		Offset:     offset,
		Limit:      limit,
		HasMore:    hasMore,
		OldestId:   oldest,
		Sources:    srcs,
		Categories: cats,
	})
}

func (ts *Teamserver) TsLogsGetPageBeforeId(beforeId int64, limit int) ([]byte, error) {
	return ts.TsLogsGetPageBeforeIdFiltered(beforeId, limit, "", "", "")
}

func (ts *Teamserver) TsLogsGetPageBeforeIdFiltered(beforeId int64, limit int, sourceFilter, categoryFilter, contains string) ([]byte, error) {
	if ts.LogManager == nil {
		return json.Marshal(LogPage{Items: []adaptix.LogEntry{}, Total: 0, Offset: 0, Limit: limit, Sources: []string{}, Categories: map[string][]string{}})
	}
	items, total := ts.LogManager.PageBeforeIdFiltered(beforeId, limit, sourceFilter, categoryFilter, contains)
	srcs, cats := ts.logCatalogSnapshot()
	var oldest int64
	for _, e := range items {
		if e.Id > 0 && (oldest == 0 || e.Id < oldest) {
			oldest = e.Id
		}
	}
	hasMore := false
	if oldest > 0 {
		hasMore = ts.LogManager.HasOlderThan(oldest, sourceFilter, categoryFilter, contains)
	}
	return json.Marshal(LogPage{
		Items:      items,
		Total:      total,
		Offset:     0,
		Limit:      limit,
		HasMore:    hasMore,
		OldestId:   oldest,
		Sources:    srcs,
		Categories: cats,
	})
}
