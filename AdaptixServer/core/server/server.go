package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"fmt"
	"os"
)

func NewTeamserver() *Teamserver {

	dbms, err := database.NewDatabase(logs.RepoLogsInstance.DbPath)
	if err != nil {
		logs.Error("", "Failed to create a DBMS: "+err.Error())
		return nil
	}

	ts := &Teamserver{
		Profile:          profile.NewProfile(),
		DBMS:             dbms,
		clients:          safe.NewMap(),
		events:           safe.NewSlice(),
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

	logs.Info("", "Restore data from Database...")

	countAgents := 0
	restoreAgents := ts.DBMS.DbAgentAll()
	for _, agentData := range restoreAgents {

		agent := &Agent{
			Data:        agentData,
			TasksQueue:  safe.NewSlice(),
			Tasks:       safe.NewMap(),
			ClosedTasks: safe.NewMap(),
			Tick:        false,
		}

		ts.agents.Put(agentData.Id, agent)

		packet := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packet)

		message := fmt.Sprintf("Restore '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
		packet2 := CreateSpEvent(EVENT_AGENT_NEW, message)
		ts.TsSyncAllClients(packet2)
		ts.events.Put(packet2)

		restoreTasks := ts.DBMS.DbTasksAll(agentData.Id)
		for _, taskData := range restoreTasks {
			agent.ClosedTasks.Put(taskData.TaskId, taskData)

			packet1 := CreateSpAgentTaskCreate(taskData)
			ts.TsSyncAllClients(packet1)

			packet2 := CreateSpAgentTaskUpdate(taskData)
			ts.TsSyncAllClients(packet2)
		}

		countAgents++
	}
	logs.Success("   ", "Restored %v agents", countAgents)

	countDownloads := 0
	restoreDownloads := ts.DBMS.DbDownloadAll()
	for _, restoreDownload := range restoreDownloads {
		ts.downloads.Put(restoreDownload.FileId, restoreDownload)

		packetRes1 := CreateSpDownloadCreate(restoreDownload)
		ts.TsSyncAllClients(packetRes1)

		packetRes2 := CreateSpDownloadUpdate(restoreDownload)
		ts.TsSyncAllClients(packetRes2)

		countDownloads++
	}
	logs.Success("   ", "Restored %v downloads", countDownloads)

	countListeners := 0
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerType, restoreListener.ListenerConfig, restoreListener.CustomData)
		if err != nil {
			logs.Error("", "Failed to restore listener %s: %s", restoreListener.ListenerName, err.Error())
		} else {
			countListeners++
		}
	}
	logs.Success("   ", "Restored %v listeners", countListeners)

}

func (ts *Teamserver) Start() {
	var (
		stopped chan bool
		err     error
	)

	ts.AdaptixServer, err = connector.NewTsConnector(ts, *ts.Profile.Server)
	if err != nil {
		logs.Error("", "Failed to init HTTP handler: "+err.Error())
		return
	}

	go ts.AdaptixServer.Start(&stopped)
	logs.Success("", "Starting server -> https://%s:%v%s", "0.0.0.0", ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	ts.RestoreData()

	go ts.TsAgentTickUpdate()

	<-stopped
	logs.Warn("", "Teamserver finished")
	os.Exit(0)
}
