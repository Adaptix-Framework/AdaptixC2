package database

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"AdaptixServer/core/utils/logs"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbDownloadExist(fileId string) bool {
	rows, err := dbms.database.Query("SELECT FileId FROM Downloads WHERE FileId = ?;", fileId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbDownloadInsert(downloadData adaptix.DownloadData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbDownloadExist(downloadData.FileId)
	if ok {
		return fmt.Errorf("download %s already exists", downloadData.FileId)
	}

	insertQuery := `INSERT INTO Downloads (FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State) values(?,?,?,?,?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery,
		downloadData.FileId, downloadData.AgentId, downloadData.AgentName, downloadData.User, downloadData.Computer, downloadData.RemotePath,
		downloadData.LocalPath, downloadData.TotalSize, downloadData.RecvSize, downloadData.Date, downloadData.State,
	)
	return err
}

func (dbms *DBMS) DbDownloadDelete(fileId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Downloads WHERE FileId = ?;`
	_, err := dbms.database.Exec(deleteQuery, fileId)
	return err
}

func (dbms *DBMS) DbDownloadDeleteBatch(fileIds []string) error {
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

func (dbms *DBMS) DbDownloadAll() []adaptix.DownloadData {
	var downloads []adaptix.DownloadData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State FROM Downloads;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query downloads: "+err.Error())
			return downloads
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			downloadData := adaptix.DownloadData{}
			err = query.Scan(&downloadData.FileId, &downloadData.AgentId, &downloadData.AgentName, &downloadData.User, &downloadData.Computer, &downloadData.RemotePath,
				&downloadData.LocalPath, &downloadData.TotalSize, &downloadData.RecvSize, &downloadData.Date, &downloadData.State,
			)
			if err != nil {
				continue
			}
			downloads = append(downloads, downloadData)
		}
	}
	return downloads
}
