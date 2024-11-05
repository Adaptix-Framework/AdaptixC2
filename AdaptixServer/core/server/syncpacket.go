package server

import "time"

const (
	STORE_LOG  = "log"
	STORE_TICK = "tick"
	STORE_INIT = "init"

	TYPE_SYNC_START  = 0x11
	TYPE_SYNC_FINISH = 0x12

	TYPE_CLIENT_CONNECT    = 0x21
	TYPE_CLIENT_DISCONNECT = 0x22

	TYPE_LISTENER_REG   = 0x31
	TYPE_LISTENER_START = 0x32
	TYPE_LISTENER_STOP  = 0x33
	TYPE_LISTENER_EDIT  = 0x34

	TYPE_AGENT_REG  = 0x41
	TYPE_AGENT_NEW  = 0x42
	TYPE_AGENT_TICK = 0x43
)

/// SYNC

func CreateSpSyncStart(count int) SyncPackerStart {
	return SyncPackerStart{
		SpType: TYPE_SYNC_START,

		Count: count,
	}
}

func CreateSpSyncFinish() SyncPackerFinish {
	return SyncPackerFinish{
		SpType: TYPE_SYNC_FINISH,
	}
}

/// CLIENT

func CreateSpClientConnect(username string) SyncPackerClientConnect {
	return SyncPackerClientConnect{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_CLIENT_CONNECT,

		Username: username,
	}
}

func CreateSpClientDisconnect(username string) SyncPackerClientDisconnect {
	return SyncPackerClientDisconnect{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_CLIENT_DISCONNECT,

		Username: username,
	}
}

/// LISTENER

func CreateSpListenerReg(fn string, ui string) SyncPackerListenerReg {
	return SyncPackerListenerReg{
		store:  STORE_INIT,
		SpType: TYPE_LISTENER_REG,

		ListenerFN: fn,
		ListenerUI: ui,
	}
}

func CreateSpListenerStart(listenerData ListenerData) SyncPackerListenerStart {
	return SyncPackerListenerStart{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_LISTENER_START,

		ListenerName:   listenerData.Name,
		ListenerType:   listenerData.Type,
		BindHost:       listenerData.BindHost,
		BindPort:       listenerData.BindPort,
		AgentHost:      listenerData.AgentHost,
		AgentPort:      listenerData.AgentPort,
		ListenerStatus: listenerData.Status,
		Data:           listenerData.Data,
	}
}

func CreateSpListenerEdit(listenerData ListenerData) SyncPackerListenerStart {
	packet := CreateSpListenerStart(listenerData)
	packet.SpType = TYPE_LISTENER_EDIT
	return packet
}

func CreateSpListenerStop(name string) SyncPackerListenerStop {
	return SyncPackerListenerStop{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_LISTENER_STOP,

		ListenerName: name,
	}
}

/// AGENT

func CreateSpAgentReg(agent string, listener string, ui string, cmd string) SyncPackerAgentReg {
	return SyncPackerAgentReg{
		store:  STORE_INIT,
		SpType: TYPE_AGENT_REG,

		Agent:    agent,
		Listener: listener,
		AgentUI:  ui,
		AgentCmd: cmd,
	}
}

func CreateSpAgentNew(agentData AgentData) SyncPackerAgentNew {
	return SyncPackerAgentNew{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_NEW,

		Id:         agentData.Id,
		Name:       agentData.Name,
		Listener:   agentData.Listener,
		Async:      agentData.Async,
		ExternalIP: agentData.ExternalIP,
		InternalIP: agentData.InternalIP,
		GmtOffset:  agentData.GmtOffset,
		Sleep:      agentData.Sleep,
		Jitter:     agentData.Jitter,
		Pid:        agentData.Pid,
		Tid:        agentData.Tid,
		Arch:       agentData.Arch,
		Elevated:   agentData.Elevated,
		Process:    agentData.Process,
		Os:         agentData.Os,
		OsDesc:     agentData.OsDesc,
		Domain:     agentData.Domain,
		Computer:   agentData.Computer,
		Username:   agentData.Username,
		LastTick:   agentData.LastTick,
		Tags:       agentData.Tags,
	}
}

func CreateSpAgentTick(AgentID string) SyncPackerAgentTick {
	return SyncPackerAgentTick{
		store:  STORE_TICK,
		SpType: TYPE_AGENT_TICK,

		Id: AgentID,
	}
}
