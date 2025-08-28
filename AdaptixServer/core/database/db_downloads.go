package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbDownloadExist(fileId string) bool {
	rows, err := dbms.database.Query("SELECT FileId FROM Downloads;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowFileId := ""
		_ = rows.Scan(&rowFileId)
		if fileId == rowFileId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbDownloadInsert(downloadData adaptix.DownloadData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbDownloadExist(downloadData.FileId)
	if ok {
		return fmt.Errorf("download %s alredy exists", downloadData.FileId)
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
		return errors.New("database not exists")
	}

	ok = dbms.DbDownloadExist(fileId)
	if !ok {
		return fmt.Errorf("download %s does not exists", fileId)
	}

	deleteQuery := `DELETE FROM Downloads WHERE FileId = ?;`
	_, err := dbms.database.Exec(deleteQuery, fileId)

	return err
}

func (dbms *DBMS) DbDownloadAll() []adaptix.DownloadData {
	var downloads []adaptix.DownloadData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State FROM Downloads;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

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
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return downloads
}
