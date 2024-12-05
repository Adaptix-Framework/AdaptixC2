package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
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
		agents:           safe.NewMap(),
		downloads:        safe.NewMap(),
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
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerType, restoreListener.ListenerConfig)
		if err != nil {
			logs.Error("Failed to restore listener %s: %s", restoreListener.ListenerName, err.Error())
		} else {
			countListeners++
		}
	}
	logs.Success("Restored %v listeners", countListeners)

	countDownloads := 0
	restoreDownloads := ts.DBMS.DbDownloadAll()
	for _, restoreDownload := range restoreDownloads {
		ts.downloads.Put(restoreDownload.FileId, restoreDownload)

		packet := CreateSpDownloadCreate(restoreDownload)
		ts.TsSyncAllClients(packet)
		ts.TsSyncSavePacket(packet.store, packet)

		packetRes1 := CreateSpDownloadCreate(restoreDownload)
		ts.TsSyncAllClients(packetRes1)
		ts.TsSyncSavePacket(packetRes1.store, packetRes1)

		packetRes2 := CreateSpDownloadUpdate(restoreDownload)
		ts.TsSyncAllClients(packetRes2)
		ts.TsSyncSavePacket(packetRes2.store, packetRes2)

		countDownloads++
	}
	logs.Success("Restored %v downloads", countDownloads)

}

func (ts *Teamserver) Start() {
	var (
		stopped chan bool
		err     error
	)

	ts.AdaptixServer, err = connector.NewTsConnector(ts, *ts.Profile.Server)
	if err != nil {
		logs.Error("Failed to init HTTP handler: " + err.Error())
		return
	}

	go ts.AdaptixServer.Start(&stopped)
	logs.Success("Starting server -> https://%s:%v%s", "0.0.0.0", ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	ts.RestoreData()

	<-stopped
	logs.Warn("Teamserver finished")
	os.Exit(0)
}
