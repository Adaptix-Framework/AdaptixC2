package server

import (
	"AdaptixServer/core/axscript"
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/idgen"
	"AdaptixServer/core/utils/token"
	"context"
	"encoding/hex"
	"fmt"
	"net"
	"os"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/Adaptix-Framework/axsafe"
	"github.com/goccy/go-yaml"
)

func NewTeamserver(debug bool) *Teamserver {
	ctx, cancel := context.WithCancel(context.Background())
	ts := &Teamserver{
		ctx:        ctx,
		cancel:     cancel,
		LogManager: NewLogManager(debug),
	}
	ts.LogManager.Bind(ts)

	paths, err := initPaths()
	if err != nil {
		fmt.Fprintf(os.Stderr, "[-] Failed to init paths: %s\n", err.Error())
		return nil
	}
	ts.Paths = paths

	dbms, err := database.NewDatabase(paths.DbPath, ts)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "", "Failed to create a DBMS: %s", err.Error())
		return nil
	}
	ts.DBMS = dbms

	broker := NewMessageBroker()
	broker.Start()
	ts.Broker = broker

	ts.Profile = profile.NewProfile(ts)
	ts.EventManager = eventing.NewEventManager(ts)
	ts.OTPManager = token.NewOTPManager(60*time.Second, 30*time.Second)

	ts.listener_configs = axsafe.NewMap[string, extender.ListenerInfo]()
	ts.agent_configs = axsafe.NewMap[string, extender.AgentInfo]()
	ts.service_configs = axsafe.NewMap[string, extender.ServiceInfo]()

	ts.wm_agent_types = axsafe.NewMap[string, string]()
	ts.wm_listeners = axsafe.NewMap[string, []string]()

	ts.notifications = axsafe.NewSlice()
	ts.Agents = axsafe.NewMap[int64, *adaptix.Agent]()
	ts.agentsUid = axsafe.NewMap[string, int64]()
	ts.listeners = axsafe.NewMap[string, adaptix.ListenerData]()
	ts.downloads = axsafe.NewMap[int64, adaptix.TransferData]()
	ts.uploads = axsafe.NewMap[int64, adaptix.TransferData]()
	ts.tmp_uploads = axsafe.NewMap[int64, string]()
	ts.terminals = axsafe.NewMap[int64, *Terminal]()
	ts.pivots = axsafe.NewSlice()
	ts.groups = axsafe.NewMap[int64, adaptix.GroupData]()
	ts.builders = axsafe.NewMap[string, *AgentBuilder]()

	ts.tickedAgents = axsafe.NewSet[int64]()
	ts.tickNotify = make(chan struct{}, 1)

	ts.IdGen = idgen.New("screen", "cred", "target", "task", "file", "agent", "listener", "payload")
	_ = ts.IdGen.Bind(dbms.GetDB())

	ts.ScriptManager = axscript.NewScriptManager(ts)
	ts.initEventHandlerRegistry()
	ts.TaskManager = NewTaskManager(ts)
	ts.TunnelManager = NewTunnelManager(ts)
	ts.TunnelManager.Start(ts.ctx)
	ts.FrameManager = NewFrameManager(ts, nil)
	ts.Extender = extender.NewExtender(ts, ts.Paths.ListenerPath)
	return ts
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

	ts.TsLogAdd(adaptix.LogStatusInfo, 0, "server", "", "Restore data from Database...")

	/// AGENTS
	countAgents := 0
	restoreAgents := ts.DBMS.DbAgentAll()
	for _, agentData := range restoreAgents {

		agentFunctions, err := ts.Extender.ExAgentRestore(agentData.Name, agentData)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusWarn, 1, "server", "", "Failed to get agentFunctions for agent %v (%v): %v", agentData.Id, agentData.Name, err.Error())
			continue
		}

		agent := adaptix.NewAgent(agentData, agentFunctions)

		if agentData.Mark == "Terminated" {
			agent.SetActive(false)
		} else if agentData.Mark == "" {
			if !agentData.Async {
				agentData.Mark = "Disconnect"
			}
		}

		ts.Agents.Put(agentData.Id, agent)
		if len(agentData.UID) > 0 {
			ts.agentsUid.Put(hex.EncodeToString(agentData.UID), agentData.Id)
		}

		packet := CreateSpAgentNew(agentData)
		ts.TsSyncAllClients(packet)

		ts.TsNotifyAgent(true, agentData)

		countAgents++
	}
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v agents", countAgents)

	/// PIVOT
	countPivots := 0
	restorePivots := ts.DBMS.DbPivotAll()
	for _, restorePivot := range restorePivots {
		_ = ts.TsPivotCreate(restorePivot.PivotId, restorePivot.ParentAgentId, restorePivot.ChildAgentId, restorePivot.PivotName, true)
		countPivots++
	}
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v pivots", countPivots)

	/// GROUPS
	countGroups := 0
	restoreGroups := ts.DBMS.DbGroupGetAll("")
	for _, g := range restoreGroups {
		ts.groups.Put(g.GroupId, g)
		countGroups++
	}
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v groups", countGroups)

	/// TUNNELS
	countTunnels := 0
	for _, row := range ts.DBMS.DbTunnelAll() {
		data := row.Data
		data.Active = false
		agent, agentOk := ts.Agents.Get(data.AgentId)
		var cbs adaptix.TunnelCallbacks
		if agentOk && agent != nil {
			cbs = agent.Fn.TunnelCB
		}
		ts.TunnelManager.PutTunnel(&Tunnel{
			Data:      data,
			Type:      row.TypeCode,
			Active:    false,
			Callbacks: cbs,
		})
		countTunnels++
	}
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v tunnels (paused)", countTunnels)
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v listeners", ts.DBMS.DbTableCount("Listeners"))
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v screenshots", ts.DBMS.DbTableCount("Screenshots"))
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v downloads", ts.DBMS.DbTableCount("Downloads"))
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v credentials", ts.DBMS.DbTableCount("Credentials"))
	ts.TsLogAdd(adaptix.LogStatusSuccess, 1, "server", "", "Restored %v targets", ts.DBMS.DbTableCount("Targets"))

	/// LISTENERS
	restoreListeners := ts.DBMS.DbListenerAll()
	for _, restoreListener := range restoreListeners {
		err = ts.TsListenerStart(restoreListener.ListenerName, restoreListener.ListenerRegName, restoreListener.ListenerConfig, restoreListener.CreateTime, restoreListener.Watermark, restoreListener.CustomData, restoreListener.Tags)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "", "Failed to restore listener %s: %s", restoreListener.ListenerName, err.Error())
		} else {
			listenerData, ok := ts.listeners.Get(restoreListener.ListenerName)
			if ok {
				if restoreListener.ListenerStatus == "Paused" && listenerData.Status == "Listen" {
					err = ts.Extender.ExListenerPause(restoreListener.ListenerName)
					if err != nil {
						ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "", "Failed to pause restored listener %s: %s", restoreListener.ListenerName, err.Error())
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
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "", "Failed to init HTTP handler: %s", err.Error())
		return
	}

	ts.Extender.LoadPlugins(ts.Profile.Server.Extenders)

	ts.TsAxScriptLoadFromProfile()
	ts.loadEventMutes()
	ts.TsEventHandlersLoad()

	go ts.AdaptixServer.Start(&stopped)
	ts.TsLogAdd(adaptix.LogStatusSuccess, 0, "server", "", "Starting server -> https://%s:%v%s", ts.Profile.Server.Interface, ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	ts.RestoreData()
	ts.TsLogAdd(adaptix.LogStatusSuccess, 0, "server", "", "The AdaptixC2 server is ready")

	go ts.TsAgentTickUpdate(ts.ctx)

	<-stopped
	ts.EventManager.Shutdown()
	ts.LogManager.Stop()
	ts.FrameManager.Stop()
	ts.cancel()
	ts.TsLogAdd(adaptix.LogStatusWarn, 0, "server", "", "Teamserver finished")
}
