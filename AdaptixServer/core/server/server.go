package server

import (
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"os"
)

func NewTeamserver() *Teamserver {

	dbms, err := database.NewDatabase(logs.RepoLogsInstance.DbPath)
	if err != nil {
		logs.Error("Failed to create a DBMS: " + err.Error())
		return nil
	}

	ts := &Teamserver{
		Profile:          profile.NewProfile(),
		DBMS:             dbms,
		clients:          safe.NewMap(),
		syncpackets:      safe.NewMap(),
		listener_configs: safe.NewMap(),
		agent_configs:    safe.NewMap(),
		agent_types:      safe.NewMap(),
		listeners:        safe.NewMap(),
	}
	ts.Extender = extender.NewExtender(ts)
	return ts
}

func (ts *Teamserver) SetSettings(port int, endpoint string, password string, cert string, key string, ext string) {
	ts.Profile.Server = &profile.TsProfile{
		Port:     port,
		Endpoint: endpoint,
		Password: password,
		Cert:     cert,
		Key:      key,
		Ext:      ext,
	}
}

func (ts *Teamserver) SetProfile(path string) error {
	var (
		err        error
		fileConten []byte
	)

	fileConten, err = os.ReadFile(path)
	if err != nil {
		return err
	}

	err = json.Unmarshal(fileConten, &ts.Profile)
	if err != nil {
		return err
	}

	return nil
}

func (ts *Teamserver) RestoreData() {
	var (
		ok  bool
		err error
	)

	ok = ts.DBMS.DatabaseExists()
	if !ok {
		return
	}

	logs.Info("Restore data from Database...")

	countListeners := 0
	restoreListeners := ts.DBMS.ListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.ListenerStart(restoreListener.ListenerName, restoreListener.ListenerType, restoreListener.ListenerConfig)
		if err != nil {
			logs.Error("Failed to restore listener %s: %s", restoreListener.ListenerName, err.Error())
		} else {
			countListeners++
		}
	}

	logs.Success("Restored %v listeners", countListeners)
}

func (ts *Teamserver) Start() {
	var (
		stoped chan bool
		err    error
	)

	ts.AdaptixServer, err = httphandler.NewTsHttpHandler(ts, *ts.Profile.Server)
	if err != nil {
		logs.Error("Failed to init HTTP handler: " + err.Error())
		return
	}

	go ts.AdaptixServer.Start(&stoped)
	logs.Success("Starting server -> https://%s:%v%s", "0.0.0.0", ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	ts.RestoreData()

	<-stoped
	logs.Warn("Teamserver finished")
	os.Exit(0)
}
