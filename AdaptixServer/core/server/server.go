package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"io/fs"
	"net"
	"os"
	"path/filepath"
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

		events:      safe.NewSlice(),
		clients:     safe.NewMap(),
		agents:      safe.NewMap(),
		listeners:   safe.NewMap(),
		messages:    safe.NewSlice(),
		downloads:   safe.NewMap(),
		tmp_uploads: safe.NewMap(),
		screenshots: safe.NewMap(),
		credentials: safe.NewSlice(),
		targets:     safe.NewSlice(),
		tunnels:     safe.NewMap(),
		terminals:   safe.NewMap(),
		pivots:      safe.NewSlice(),
		otps:        safe.NewMap(),
	}
	ts.Extender = extender.NewExtender(ts)
	return ts
}

func (ts *Teamserver) SetSettings(host string, port int, endpoint string, password string, cert string, key string, extenders []string) {
	ts.Profile.Server = &profile.TsProfile{
		Interface:  host,
		Port:       port,
		Endpoint:   endpoint,
		Password:   password,
		Cert:       cert,
		Key:        key,
		Extenders:  extenders,
		ATokenLive: 12,
		RTokenLive: 168,
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
			Data:              agentData,
			OutConsole:        safe.NewSlice(),
			HostedTunnelData:  safe.NewSafeQueue(0x1000),
			HostedTasks:       safe.NewSafeQueue(0x100),
			HostedTunnelTasks: safe.NewSafeQueue(0x100),
			RunningTasks:      safe.NewMap(),
			RunningJobs:       safe.NewMap(),
			CompletedTasks:    safe.NewMap(),
			PivotParent:       nil,
			PivotChilds:       safe.NewSlice(),
			Tick:              false,
			Active:            true,
		}

		if agent.Data.Mark == "Terminated" {
			agent.Active = false
		}

		if agent.Data.Mark == "" {
			if !agent.Data.Async {
				agent.Data.Mark = "Disconnect"
			}
		}

		ts.agents.Put(agentData.Id, agent)

		packet := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packet)

		ts.TsEventAgent(true, agentData)

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

	/// CHAT
	countMessages := 0
	restoreChat := ts.DBMS.DbChatAll()
	for _, restoreMessage := range restoreChat {
		ts.messages.Put(restoreMessage)
		packet := CreateSpChatMessage(restoreMessage)
		ts.TsSyncAllClients(packet)
		countMessages++
	}
	logs.Success("   ", "Restored %v messages", countMessages)

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

	/// SCREENSHOTS
	countScreenshots := 0
	restoreScreenshots := ts.DBMS.DbScreenshotAll()
	for _, restoreScreen := range restoreScreenshots {

		restoreScreen.Content, err = os.ReadFile(restoreScreen.LocalPath)
		if err != nil {
			continue
		}

		ts.screenshots.Put(restoreScreen.ScreenId, restoreScreen)

		packet := CreateSpScreenshotCreate(restoreScreen)
		ts.TsSyncAllClients(packet)

		countScreenshots++
	}
	logs.Success("   ", "Restored %v screens", countScreenshots)

	/// CREDENTIALS
	countCredentials := 0
	restoreCredentials := ts.DBMS.DbCredentialsAll()
	for _, restoreCredential := range restoreCredentials {

		ts.credentials.Put(restoreCredential)
		countCredentials++
	}
	packetCreds := CreateSpCredentialsAdd(restoreCredentials)
	ts.TsSyncAllClients(packetCreds)
	logs.Success("   ", "Restored %v credentials", countCredentials)

	/// TARGETS
	countTargets := 0
	restoreTargets := ts.DBMS.DbTargetsAll()
	for _, restoreTarget := range restoreTargets {
		ts.targets.Put(restoreTarget)
		countTargets++
	}
	packetTargets := CreateSpTargetsAdd(restoreTargets)
	ts.TsSyncAllClients(packetTargets)
	logs.Success("   ", "Restored %v targets", countTargets)

	/// LISTENERS
	countListeners := 0
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		// 跳过未注册的监听器类型（避免“幽灵条目”）
		if !ts.listener_configs.Contains(restoreListener.ListenerRegName) {
			logs.Error("", "Skip restore listener %s: reg '%s' is not registered", restoreListener.ListenerName, restoreListener.ListenerRegName)
			continue
		}
		// 跳过内存中已存在的监听器（防重复恢复）
		if ts.listeners.Contains(restoreListener.ListenerName) {
			logs.Warn("", "Skip restore listener %s: already present in memory", restoreListener.ListenerName)
			continue
		}

		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerRegName, restoreListener.ListenerConfig, restoreListener.Watermark, restoreListener.CustomData)
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

	interfaces, err := net.Interfaces()
	if err == nil {
		ts.Parameters.Interfaces = append(ts.Parameters.Interfaces, "0.0.0.0")
		for _, i := range interfaces {
			iAddrs, err := i.Addrs()
			if err == nil {
				for _, addr := range iAddrs {
					ipNet, ok := addr.(*net.IPNet)
					if ok {
						if ipNet.IP.To4() != nil {
							ts.Parameters.Interfaces = append(ts.Parameters.Interfaces, ipNet.IP.String())
						}
					}
				}
			}
		}
	}
	if len(ts.Parameters.Interfaces) == 0 {
		ts.Parameters.Interfaces = append(ts.Parameters.Interfaces, "0.0.0.0")
		ts.Parameters.Interfaces = append(ts.Parameters.Interfaces, "127.0.0.1")
	}

	ts.AdaptixServer, err = connector.NewTsConnector(ts, *ts.Profile.Server, *ts.Profile.ServerResponse)
	if err != nil {
		logs.Error("", "Failed to init HTTP handler: "+err.Error())
		return
	}

	go ts.AdaptixServer.Start(&stopped)
	logs.Success("", "Starting server -> https://%s:%v%s", ts.Profile.Server.Interface, ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	// Build extenders list: use profile, plus auto-scan release/extenders for config.json as fallback/augmentation
	extenders := ts.Profile.Server.Extenders
	// Auto-scan local release/extenders directory for any config.json files
	{
		baseDir := filepath.Join("release", "extenders")
		var found []string
		_ = filepath.WalkDir(baseDir, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				return nil
			}
			if d.IsDir() {
				return nil
			}
			if filepath.Base(path) == "config.json" {
				found = append(found, path)
			}
			return nil
		})
		if len(found) > 0 {
			extenders = append(extenders, found...)
			logs.Success("", "Auto-scanned %d extender configs from %s", len(found), baseDir)
		} else {
			logs.Warn("", "No extender configs found by auto-scan in %s", baseDir)
		}
	}

	// Load extenders/plugins before restoring data to ensure registration is available for clients
	ts.Extender.LoadPlugins(extenders)
	logs.Success("", "Extender load requested for %d configs", len(extenders))
	for i, p := range extenders {
		logs.Info("   ", "Extender[%d]: %s", i, p)
	}

	// Broadcast registered listeners to already-connected clients, so "Create Listener" dropdown updates immediately
	countReg := 0
	ts.listener_configs.ForEach(func(key string, value interface{}) bool {
		listenerInfo := value.(extender.ListenerInfo)
		axLen := len(listenerInfo.AX)
		logs.Info("", "RegisterListener -> Name=%s Type=%s Protocol=%s AX_len=%d", listenerInfo.Name, listenerInfo.Type, listenerInfo.Protocol, axLen)
		p := CreateSpListenerReg(listenerInfo.Name, listenerInfo.Protocol, listenerInfo.Type, listenerInfo.AX)
		ts.TsSyncAllClients(p)
		countReg++
		return true
	})
	logs.Success("", "Broadcasted %d registered listeners", countReg)

	ts.RestoreData()
	logs.Success("", "The AdaptixC2 server is ready")

	go ts.TsAgentTickUpdate()

	<-stopped
	logs.Warn("", "Teamserver finished")
	os.Exit(0)
}
