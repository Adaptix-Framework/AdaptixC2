package database

import (
	"errors"

	"github.com/Adaptix-Framework/axc2/v2"
)

type TunnelRow struct {
	Data     adaptix.TunnelData
	TypeCode int
}

func (dbms *DBMS) DbTunnelExist(tunnelId int64) bool {
	rows, err := dbms.database.Query(`SELECT TunnelId FROM Tunnels WHERE TunnelId = ?;`, tunnelId)
	if err != nil {
		return false
	}
	defer func() { _ = rows.Close() }()
	return rows.Next()
}

func (dbms *DBMS) DbTunnelInsert(data adaptix.TunnelData, typeCode int) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	q := `INSERT OR REPLACE INTO Tunnels (TunnelId, AgentId, Computer, Username, Process, Type, TypeCode, Info, Interface, Port, Client, Fhost, Fport, AuthUser, AuthPass, Date, Active) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(q, data.TunnelId, data.AgentId, data.Computer, data.Username, data.Process, data.Type, typeCode, data.Info, data.Interface, data.Port, data.Client, data.Fhost, data.Fport, data.AuthUser, data.AuthPass, data.Date, data.Active)
	return err
}

func (dbms *DBMS) DbTunnelUpdate(data adaptix.TunnelData, typeCode int) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	if !dbms.DbTunnelExist(data.TunnelId) {
		return dbms.DbTunnelInsert(data, typeCode)
	}
	q := `UPDATE Tunnels SET AgentId=?, Computer=?, Username=?, Process=?, Type=?, TypeCode=?, Info=?, Interface=?, Port=?, Client=?, Fhost=?, Fport=?, AuthUser=?, AuthPass=?, Date=?, Active=? WHERE TunnelId=?;`
	_, err := dbms.database.Exec(q, data.AgentId, data.Computer, data.Username, data.Process, data.Type, typeCode, data.Info, data.Interface, data.Port, data.Client, data.Fhost, data.Fport, data.AuthUser, data.AuthPass, data.Date, data.Active, data.TunnelId)
	return err
}

func (dbms *DBMS) DbTunnelDelete(tunnelId int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`DELETE FROM Tunnels WHERE TunnelId = ?;`, tunnelId)
	return err
}

func (dbms *DBMS) DbTunnelAll() []TunnelRow {
	var out []TunnelRow
	if !dbms.DatabaseExists() {
		return out
	}
	q := `SELECT TunnelId, AgentId, Computer, Username, Process, Type, TypeCode, Info, Interface, Port, Client, Fhost, Fport, AuthUser, AuthPass, Date, Active FROM Tunnels;`
	rows, err := dbms.database.Query(q)
	if err != nil {
		dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query tunnels: %s", err.Error())
		return out
	}
	defer func() { _ = rows.Close() }()

	for rows.Next() {
		var r TunnelRow
		var active bool
		err = rows.Scan(&r.Data.TunnelId, &r.Data.AgentId, &r.Data.Computer, &r.Data.Username, &r.Data.Process, &r.Data.Type, &r.TypeCode, &r.Data.Info, &r.Data.Interface, &r.Data.Port, &r.Data.Client, &r.Data.Fhost, &r.Data.Fport, &r.Data.AuthUser, &r.Data.AuthPass, &r.Data.Date, &active)
		if err != nil {
			if dbms.ts != nil {
				dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to scan tunnel row: %s", err.Error())
			}
			continue
		}
		r.Data.Active = active
		out = append(out, r)
	}
	return out
}
