package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"net"
	"os"

	"github.com/Adaptix-Framework/axc2"
	"github.com/goccy/go-yaml"
)

func NewTeamserver() *Teamserver {

	dbms, err := database.NewDatabase(logs.RepoLogsInstance.DbPath)
	if err != nil {
		logs.Error("", "Failed to create a DBMS: "+err.Error())
		return nil
	}

	broker := NewMessageBroker()
	broker.Start()

	ts := &Teamserver{
		Profile:      profile.NewProfile(),
		DBMS:         dbms,
		Broker:       broker,
		EventManager: eventing.NewEventManager(),

		listener_configs: safe.NewMap(),
		agent_configs:    safe.NewMap(),
		service_configs:  safe.NewMap(),

		wm_agent_types: make(map[string]string),
		wm_listeners:   make(map[string][]string),

		notifications: safe.NewSlice(),
		Agents:        safe.NewMap(),
		listeners:     safe.NewMap(),
		downloads:     safe.NewMap(),
		tmp_uploads:   safe.NewMap(),
		terminals:     safe.NewMap(),
		pivots:        safe.NewSlice(),
		otps:          safe.NewMap(),
		builders:      safe.NewMap(),
	}
	ts.TaskManager = NewTaskManager(ts)
	ts.TunnelManager = NewTunnelManager(ts)
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
	ts.Profile.HttpServer = &profile.TsHttpServer{
		Error: &profile.TsHttpError{
			Status:      404,
			Headers:     map[string]string{},
			PagePath:    "",
			PageContent: "",
		},
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

	err = yaml.Unmarshal(fileContent, ts.Profile)
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

		extenderAgent, err := ts.Extender.ExAgentGetExtender(agentData.Name)
		if err != nil {
			logs.Warn("   ", "Failed to get extenderAgent for agent %v (%v): %v", agentData.Id, agentData.Name, err.Error())
			continue
		}

		agent := &Agent{
			Extender:          extenderAgent,
			HostedTunnelData:  safe.NewSafeQueue(0x1000),
			HostedTasks:       safe.NewSafeQueue(0x100),
			HostedTunnelTasks: safe.NewSafeQueue(0x100),
			RunningTasks:      safe.NewMap(),
			RunningJobs:       safe.NewMap(),
			PivotParent:       nil,
			PivotChilds:       safe.NewSlice(),
			Tick:              false,
			Active:            true,
		}

		if agentData.Mark == "Terminated" {
			agent.Active = false
		}

		if agentData.Mark == "" {
			if !agentData.Async {
				agentData.Mark = "Disconnect"
			}
		}

		agent.SetData(agentData)
		ts.Agents.Put(agentData.Id, agent)

		packet := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packet)

		ts.TsNotifyAgent(true, agentData)

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

	logs.Success("   ", "Restored %v listeners", ts.DBMS.DbTableCount("Listeners"))
	logs.Success("   ", "Restored %v screenshots", ts.DBMS.DbTableCount("Screenshots"))
	logs.Success("   ", "Restored %v downloads", ts.DBMS.DbTableCount("Downloads"))
	logs.Success("   ", "Restored %v credentials", ts.DBMS.DbTableCount("Credentials"))
	logs.Success("   ", "Restored %v targets", ts.DBMS.DbTableCount("Targets"))

	/// LISTENERS
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerRegName, restoreListener.ListenerConfig, restoreListener.CreateTime, restoreListener.Watermark, restoreListener.CustomData)
		if err != nil {
			logs.Error("", "Failed to restore listener %s: %s", restoreListener.ListenerName, err.Error())
		} else {
			value, ok := ts.listeners.Get(restoreListener.ListenerName)
			if ok {
				listenerData := value.(adaptix.ListenerData)

				if restoreListener.ListenerStatus == "Paused" && listenerData.Status == "Listen" {
					err = ts.Extender.ExListenerPause(restoreListener.ListenerName)
					if err != nil {
						logs.Error("", "Failed to pause restored listener %s: %s", restoreListener.ListenerName, err.Error())
					} else {
						listenerData.Status = "Paused"
						ts.listeners.Put(restoreListener.ListenerName, listenerData)
						packet := CreateSpListenerEdit(listenerData)
						ts.TsSyncAllClients(packet)
					}
				}
			}
		}
	}
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

	ts.AdaptixServer, err = connector.NewTsConnector(ts, *ts.Profile.Server, *ts.Profile.HttpServer)
	if err != nil {
		logs.Error("", "Failed to init HTTP handler: "+err.Error())
		return
	}

	ts.Extender.LoadPlugins(ts.Profile.Server.Extenders)

	go ts.AdaptixServer.Start(&stopped)
	logs.Success("", "Starting server -> https://%s:%v%s", ts.Profile.Server.Interface, ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	ts.RestoreData()
	logs.Success("", "The AdaptixC2 server is ready")

	go ts.TsAgentTickUpdate()

	<-stopped
	logs.Warn("", "Teamserver finished")
	os.Exit(0)
}
