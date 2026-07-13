package server

import (
	"encoding/json"
	"fmt"
	"io"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsLogAdd(status adaptix.LogStatus, level int, source string, format string, args ...any) {
	if ts.LogManager == nil {
		return
	}
	ts.LogManager.Add(adaptix.LogEntry{
		Status:  status,
		Level:   level,
		Source:  source,
		Message: fmt.Sprintf(format, args...),
	})
}

func (ts *Teamserver) TsLogWriter(status adaptix.LogStatus, source string) io.Writer {
	if ts.LogManager == nil {
		return io.Discard
	}
	return newLogWriter(ts.LogManager, status, source)
}

type LogPage struct {
	Items  []adaptix.LogEntry `json:"items"`
	Total  int                `json:"total"`
	Offset int                `json:"offset"`
	Limit  int                `json:"limit"`
}

func (ts *Teamserver) TsLogsGetPage(offset, limit int) ([]byte, error) {
	if ts.LogManager == nil {
		return json.Marshal(LogPage{Items: []adaptix.LogEntry{}, Total: 0, Offset: offset, Limit: limit})
	}
	items, total := ts.LogManager.Page(offset, limit)
	return json.Marshal(LogPage{
		Items:  items,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

func (ts *Teamserver) TsLogsGetPageBeforeId(beforeId int64, limit int) ([]byte, error) {
	if ts.LogManager == nil {
		return json.Marshal(LogPage{Items: []adaptix.LogEntry{}, Total: 0, Offset: 0, Limit: limit})
	}
	items, total := ts.LogManager.PageBeforeId(beforeId, limit)
	return json.Marshal(LogPage{
		Items:  items,
		Total:  total,
		Offset: 0,
		Limit:  limit,
	})
}
