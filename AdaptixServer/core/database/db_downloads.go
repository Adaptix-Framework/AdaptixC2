package database

import (
	"AdaptixServer/core/utils/filter"
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

var sortableDownloadColumns = map[string]string{
	"FileId":     "FileId",
	"AgentId":    "AgentId",
	"AgentName":  "AgentName",
	"User":       "User",
	"Computer":   "Computer",
	"RemotePath": "RemotePath",
	"TotalSize":  "TotalSize",
	"RecvSize":   "RecvSize",
	"Date":       "Date",
	"State":      "State",
	"Tag":        "Tag",
}

func (dbms *DBMS) DbDownloadExist(fileId int64) bool {
	var id int64
	err := dbms.database.QueryRow("SELECT FileId FROM Downloads WHERE FileId = ? LIMIT 1;", fileId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbDownloadInsert(downloadData adaptix.TransferData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Downloads (FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State, Tag) values(?,?,?,?,?,?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery,
		downloadData.FileId, downloadData.AgentId, downloadData.AgentName, downloadData.User, downloadData.Computer, downloadData.RemotePath,
		downloadData.LocalPath, downloadData.TotalSize, downloadData.Progress, downloadData.Date, downloadData.State, downloadData.Tag,
	)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("download %d already exists", downloadData.FileId)
	}
	return nil
}

func (dbms *DBMS) DbDownloadDelete(fileId int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Downloads WHERE FileId = ?;`
	_, err := dbms.database.Exec(deleteQuery, fileId)
	return err
}

func (dbms *DBMS) DbDownloadDeleteBatch(fileIds []int64) error {
	if len(fileIds) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	placeholders := make([]string, len(fileIds))
	args := make([]interface{}, len(fileIds))
	for i, id := range fileIds {
		placeholders[i] = "?"
		args[i] = id
	}

	deleteQuery := fmt.Sprintf("DELETE FROM Downloads WHERE FileId IN (%s);",
		strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(deleteQuery, args...)
	return err
}

func (dbms *DBMS) DbDownloadGet(fileId int64) (adaptix.TransferData, error) {
	var downloadData adaptix.TransferData

	if !dbms.DatabaseExists() {
		return downloadData, errors.New("database does not exist")
	}

	selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State, Tag FROM Downloads WHERE FileId = ?;`
	err := dbms.database.QueryRow(selectQuery, fileId).Scan(&downloadData.FileId, &downloadData.AgentId, &downloadData.AgentName, &downloadData.User, &downloadData.Computer, &downloadData.RemotePath,
		&downloadData.LocalPath, &downloadData.TotalSize, &downloadData.Progress, &downloadData.Date, &downloadData.State, &downloadData.Tag,
	)
	if err != nil {
		return downloadData, fmt.Errorf("download %d not found", fileId)
	}
	return downloadData, nil
}

func (dbms *DBMS) DbDownloadAll() []adaptix.TransferData {
	var downloads []adaptix.TransferData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State, Tag FROM Downloads ORDER BY Date;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query downloads: %s", err.Error())
			return downloads
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			downloadData := adaptix.TransferData{}
			err = query.Scan(&downloadData.FileId, &downloadData.AgentId, &downloadData.AgentName, &downloadData.User, &downloadData.Computer, &downloadData.RemotePath,
				&downloadData.LocalPath, &downloadData.TotalSize, &downloadData.Progress, &downloadData.Date, &downloadData.State, &downloadData.Tag,
			)
			if err != nil {
				continue
			}
			downloads = append(downloads, downloadData)
		}
	}
	return downloads
}

func (dbms *DBMS) DbDownloadsGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string) ([]adaptix.TransferData, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}

	where := "1=1"
	var args []interface{}

	if agentId != 0 {
		where += " AND AgentId = ?"
		args = append(args, agentId)
	}

	if filterExpr != "" {
		node, err := filter.ParseFilterExpr(filterExpr)
		if err != nil {
			return nil, 0, fmt.Errorf("invalid filter: %w", err)
		}
		if node != nil {
			filterSQL, filterArgs := filter.ToSQL(node, []string{"FileId", "AgentId", "AgentName", "User", "Computer", "RemotePath", "Tag"})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "Date DESC"
	if col, ok := sortableDownloadColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	if err := dbms.database.QueryRow("SELECT COUNT(*) FROM Downloads WHERE "+where, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(
		"SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State, Tag"+
			" FROM Downloads WHERE "+where+" ORDER BY "+orderClause+" LIMIT ? OFFSET ?",
		selectArgs...,
	)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	out := make([]adaptix.TransferData, 0, limit)
	for rows.Next() {
		var d adaptix.TransferData
		err := rows.Scan(&d.FileId, &d.AgentId, &d.AgentName, &d.User, &d.Computer, &d.RemotePath, &d.LocalPath, &d.TotalSize, &d.Progress, &d.Date, &d.State, &d.Tag)
		if err != nil {
			continue
		}
		d.LocalPath = "******"
		out = append(out, d)
	}
	return out, total, nil
}

func (dbms *DBMS) DbDownloadUpdateState(fileId int64, state int, progress int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(
		"UPDATE Downloads SET State = ?, RecvSize = ? WHERE FileId = ?;",
		state, progress, fileId,
	)
	return err
}

func (dbms *DBMS) DbDownloadSetTagBatch(fileIds []int64, tag string) error {
	if len(fileIds) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	placeholders := make([]string, len(fileIds))
	args := make([]interface{}, 0, len(fileIds)+1)
	args = append(args, tag)
	for i, id := range fileIds {
		placeholders[i] = "?"
		args = append(args, id)
	}
	updateQuery := fmt.Sprintf("UPDATE Downloads SET Tag = ? WHERE FileId IN (%s);", strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(updateQuery, args...)
	return err
}
