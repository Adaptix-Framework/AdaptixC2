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
		Profile: profile.NewProfile(),
		DBMS:    dbms,

		listener_configs: safe.NewMap(),
		agent_configs:    safe.NewMap(),

		wm_agent_types: make(map[string]string),
		wm_listeners:   make(map[string][]string),

		events:    safe.NewSlice(),
		clients:   safe.NewMap(),
		agents:    safe.NewMap(),
		listeners: safe.NewMap(),
		downloads: safe.NewMap(),
		tunnels:   safe.NewMap(),
		pivots:    safe.NewSlice(),
	}
	ts.Extender = extender.NewExtender(ts)
	return ts
}

func (ts *Teamserver) SetSettings(port int, endpoint string, password string, cert string, key string, extenders []string) {
	ts.Profile.Server = &profile.TsProfile{
		Port:      port,
		Endpoint:  endpoint,
		Password:  password,
		Cert:      cert,
		Key:       key,
		Extenders: extenders,
	}
	ts.Profile.ServerResponse = &profile.TsResponse{
		Status:      404,
		Headers:     map[string]string{},
		PagePath:    "",
		PageContent: "",
	}
}

func (ts *Teamserver) SetProfile(path string) error {
	var (
		err         error
		fileContent []byte
	)

	fileContent, err = os.ReadFile(path)
	if err != nil {
		return err
	}

	err = json.Unmarshal(fileContent, &ts.Profile)
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

	/// DATABASE
	ok = ts.DBMS.DatabaseExists()
	if !ok {
		return
	}

	logs.Info("", "Restore data from Database...")

	/// AGENTS
	countAgents := 0
	restoreAgents := ts.DBMS.DbAgentAll()
	for _, agentData := range restoreAgents {

		agent := &Agent{
			Data:           agentData,
			OutConsole:     safe.NewSlice(),
			TunnelQueue:    safe.NewSlice(),
			TasksQueue:     safe.NewSlice(),
			RunningTasks:   safe.NewMap(),
			CompletedTasks: safe.NewMap(),
			PivotParent:    nil,
			PivotChilds:    safe.NewSlice(),
			Tick:           false,
			Active:         true,
		}

		if agent.Data.Mark == "Terminated" {
			agent.Active = false
		}

		ts.agents.Put(agentData.Id, agent)

		packet := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packet)

		message := fmt.Sprintf("Restore '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
		packet2 := CreateSpEvent(EVENT_AGENT_NEW, message)
		ts.TsSyncAllClients(packet2)
		ts.events.Put(packet2)

		/// Tasks
		restoreTasks := ts.DBMS.DbTasksAll(agentData.Id)
		for _, taskData := range restoreTasks {
			agent.CompletedTasks.Put(taskData.TaskId, taskData)

			packet2 := CreateSpAgentTaskSync(taskData)
			ts.TsSyncAllClients(packet2)
		}

		/// Consoles
		restoreConsoles := ts.DBMS.DbConsoleAll(agentData.Id)
		for _, message := range restoreConsoles {
			var packet3 map[string]any
			err = json.Unmarshal(message, &packet3)
			if err != nil {
				continue
			}

			agent.OutConsole.Put(packet3)

			ts.TsSyncAllClients(packet3)
		}

		countAgents++
	}
	logs.Success("   ", "Restored %v agents", countAgents)

	/// PIVOT
	countPivots := 0
	restorePivots := ts.DBMS.DbPivotAll()
	for _, restorePivot := range restorePivots {
		_ = ts.TsPivotCreate(restorePivot.PivotId, restorePivot.ParentAgentId, restorePivot.ChildAgentId, restorePivot.PivotName, true)
		countPivots++
	}
	logs.Success("   ", "Restored %v pivots", countPivots)

	/// DOWNLOADS
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

	/// LISTENERS
	countListeners := 0
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerType, restoreListener.ListenerConfig, restoreListener.Watermark, restoreListener.CustomData)
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

	ts.AdaptixServer, err = connector.NewTsConnector(ts, *ts.Profile.Server, *ts.Profile.ServerResponse)
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
