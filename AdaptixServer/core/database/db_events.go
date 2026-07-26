package database

import (
	"database/sql"
	"errors"
)

type DbEventHandler struct {
	ID          string
	Name        string
	Group       string
	Description string
	Event       string
	Script      string
	Enabled     bool
	Filters     string // JSON
	CreatedBy   string
	UpdatedBy   string
	CreatedAt   int64
	UpdatedAt   int64
	LastError   string
	LastRunAt   int64
}

func (dbms *DBMS) DbEventHandlersAll() []DbEventHandler {
	out := make([]DbEventHandler, 0)
	if !dbms.DatabaseExists() {
		return out
	}
	rows, err := dbms.database.Query(`SELECT Id, Name, GroupName, Description, Event, Script, Enabled, Filters,CreatedBy, UpdatedBy, CreatedAt, UpdatedAt, LastError, LastRunAt FROM EventHandlers ORDER BY UpdatedAt DESC, Name ASC;`)
	if err != nil {
		return out
	}
	defer rows.Close()
	for rows.Next() {
		var h DbEventHandler
		var enabled int
		var filters sql.NullString
		var lastErr sql.NullString
		if err := rows.Scan(
			&h.ID, &h.Name, &h.Group, &h.Description, &h.Event, &h.Script, &enabled, &filters,
			&h.CreatedBy, &h.UpdatedBy, &h.CreatedAt, &h.UpdatedAt, &lastErr, &h.LastRunAt,
		); err != nil {
			continue
		}
		h.Enabled = enabled != 0
		if filters.Valid {
			h.Filters = filters.String
		}
		if lastErr.Valid {
			h.LastError = lastErr.String
		}
		out = append(out, h)
	}
	return out
}

func (dbms *DBMS) DbEventHandlerUpsert(h DbEventHandler) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	enabled := 0
	if h.Enabled {
		enabled = 1
	}
	_, err := dbms.database.Exec(`INSERT INTO EventHandlers (Id, Name, GroupName, Description, Event, Script, Enabled, Filters,CreatedBy, UpdatedBy, CreatedAt, UpdatedAt, LastError, LastRunAt) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
		ON CONFLICT(Id) DO UPDATE SET
			Name=excluded.Name,
			GroupName=excluded.GroupName,
			Description=excluded.Description,
			Event=excluded.Event,
			Script=excluded.Script,
			Enabled=excluded.Enabled,
			Filters=excluded.Filters,
			UpdatedBy=excluded.UpdatedBy,
			UpdatedAt=excluded.UpdatedAt,
			LastError=excluded.LastError,
			LastRunAt=excluded.LastRunAt;`,
		h.ID, h.Name, h.Group, h.Description, h.Event, h.Script, enabled, h.Filters,
		h.CreatedBy, h.UpdatedBy, h.CreatedAt, h.UpdatedAt, h.LastError, h.LastRunAt,
	)
	return err
}

func (dbms *DBMS) DbEventHandlerDelete(id string) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`DELETE FROM EventHandlers WHERE Id = ?;`, id)
	return err
}

func (dbms *DBMS) DbEventHandlerSetEnabled(id string, enabled bool, updatedAt int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	en := 0
	if enabled {
		en = 1
	}
	_, err := dbms.database.Exec(`UPDATE EventHandlers SET Enabled = ?, UpdatedAt = ? WHERE Id = ?;`, en, updatedAt, id)
	return err
}

func (dbms *DBMS) DbEventHandlerSetLastRun(id string, lastError string, lastRunAt int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	dbms.enqueueBatchWrite(`UPDATE EventHandlers SET LastError = ?, LastRunAt = ? WHERE Id = ?;`, lastError, lastRunAt, id)
	return nil
}

func (dbms *DBMS) DbEventMutesAll() []string {
	out := make([]string, 0)
	if !dbms.DatabaseExists() {
		return out
	}
	rows, err := dbms.database.Query(`SELECT Event FROM EventMutes ORDER BY Event ASC;`)
	if err != nil {
		return out
	}
	defer rows.Close()
	for rows.Next() {
		var et string
		if err := rows.Scan(&et); err != nil {
			continue
		}
		out = append(out, et)
	}
	return out
}

func (dbms *DBMS) DbEventMuteAdd(event string) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`INSERT OR IGNORE INTO EventMutes (Event) VALUES (?);`, event)
	return err
}

func (dbms *DBMS) DbEventMuteRemove(event string) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`DELETE FROM EventMutes WHERE Event = ?;`, event)
	return err
}
