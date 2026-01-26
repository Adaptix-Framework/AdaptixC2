package database

import (
	"database/sql"
	"errors"
)

func (dbms *DBMS) DbExtenderDataSave(extenderName string, key string, value []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	upsertQuery := `INSERT INTO ExtenderData (ExtenderName, Key, Value) VALUES (?, ?, ?)
		ON CONFLICT(ExtenderName, Key) DO UPDATE SET Value = excluded.Value;`
	_, err := dbms.database.Exec(upsertQuery, extenderName, key, value)
	return err
}

func (dbms *DBMS) DbExtenderDataLoad(extenderName string, key string) ([]byte, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	var value []byte
	query := `SELECT Value FROM ExtenderData WHERE ExtenderName = ? AND Key = ?;`
	err := dbms.database.QueryRow(query, extenderName, key).Scan(&value)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return nil, nil
		}
		return nil, err
	}
	return value, nil
}

func (dbms *DBMS) DbExtenderDataDelete(extenderName string, key string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM ExtenderData WHERE ExtenderName = ? AND Key = ?;`
	_, err := dbms.database.Exec(deleteQuery, extenderName, key)
	return err
}

func (dbms *DBMS) DbExtenderDataKeys(extenderName string) ([]string, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	var keys []string
	query := `SELECT Key FROM ExtenderData WHERE ExtenderName = ?;`
	rows, err := dbms.database.Query(query, extenderName)
	if err != nil {
		return nil, err
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		var key string
		if err := rows.Scan(&key); err != nil {
			return nil, err
		}
		keys = append(keys, key)
	}
	return keys, nil
}

func (dbms *DBMS) DbExtenderDataDeleteAll(extenderName string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM ExtenderData WHERE ExtenderName = ?;`
	_, err := dbms.database.Exec(deleteQuery, extenderName)
	return err
}
