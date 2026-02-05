package database

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"AdaptixServer/core/utils/logs"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbCredentialsExist(credsId string) bool {
	rows, err := dbms.database.Query("SELECT CredId FROM Credentials WHERE CredId = ?;", credsId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()

}

func (dbms *DBMS) DbCredentialsAdd(credsData []*adaptix.CredsData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	for _, creds := range credsData {
		ok = dbms.DbCredentialsExist(creds.CredId)
		if ok {
			return fmt.Errorf("creds %s already exists", creds.CredId)
		}

		insertQuery := `INSERT INTO Credentials (CredId, Username, Password, Realm, Type, Tag, Date, Storage, AgentId, Host) values(?,?,?,?,?,?,?,?,?,?);`
		_, err := dbms.database.Exec(insertQuery, creds.CredId, creds.Username, creds.Password, creds.Realm, creds.Type,
			creds.Tag, creds.Date, creds.Storage, creds.AgentId, creds.Host)
		if err != nil {
			logs.Error("", err.Error())
			continue
		}
	}

	return nil
}

func (dbms *DBMS) DbCredentialsUpdate(credsData adaptix.CredsData) error {

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbCredentialsExist(credsData.CredId)
	if !ok {
		return fmt.Errorf("creds %s does not exist", credsData.CredId)
	}

	updateQuery := `UPDATE Credentials SET Username = ?, Password = ?, Realm = ?, Type = ?, Tag = ?, Storage = ?, Host = ? WHERE CredId = ?;`
	_, err := dbms.database.Exec(updateQuery, credsData.Username, credsData.Password, credsData.Realm, credsData.Type,
		credsData.Tag, credsData.Storage, credsData.Host, credsData.CredId)
	return err
}

func (dbms *DBMS) DbCredentialsDelete(credId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Credentials WHERE CredId = ?;`
	_, err := dbms.database.Exec(deleteQuery, credId)
	return err
}

func (dbms *DBMS) DbCredentialsDeleteBatch(credIds []string) error {
	if len(credIds) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	placeholders := make([]string, len(credIds))
	args := make([]interface{}, len(credIds))
	for i, id := range credIds {
		placeholders[i] = "?"
		args[i] = id
	}

	deleteQuery := fmt.Sprintf("DELETE FROM Credentials WHERE CredId IN (%s);", strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(deleteQuery, args...)
	return err
}

func (dbms *DBMS) DbCredentialsUpdateBatch(credsData []adaptix.CredsData) error {
	if len(credsData) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	tx, err := dbms.database.Begin()
	if err != nil {
		return err
	}

	updateQuery := `UPDATE Credentials SET Username = ?, Password = ?, Realm = ?, Type = ?, Tag = ?, Storage = ?, Host = ? WHERE CredId = ?;`
	stmt, err := tx.Prepare(updateQuery)
	if err != nil {
		_ = tx.Rollback()
		return err
	}
	defer func(stmt *sql.Stmt) {
		_ = stmt.Close()
	}(stmt)

	for _, cred := range credsData {
		_, err = stmt.Exec(cred.Username, cred.Password, cred.Realm, cred.Type, cred.Tag, cred.Storage, cred.Host, cred.CredId)
		if err != nil {
			_ = tx.Rollback()
			return err
		}
	}

	return tx.Commit()
}

func (dbms *DBMS) DbCredentialsAll() []*adaptix.CredsData {
	var creds []*adaptix.CredsData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT CredId, Username, Password, Realm, Type, Tag, Date, Storage, AgentId, Host FROM Credentials;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query credentials: "+err.Error())
			return creds
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			credsData := &adaptix.CredsData{}
			err = query.Scan(&credsData.CredId, &credsData.Username, &credsData.Password, &credsData.Realm, &credsData.Type,
				&credsData.Tag, &credsData.Date, &credsData.Storage, &credsData.AgentId, &credsData.Host)
			if err != nil {
				continue
			}
			creds = append(creds, credsData)
		}
	}
	return creds
}
