package server

import (
	"time"
)

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

	TYPE_AGENT_REG         = 0x41
	TYPE_AGENT_NEW         = 0x42
	TYPE_AGENT_TICK        = 0x43
	TYPE_AGENT_TASK_CREATE = 0x44
	TYPE_AGENT_TASK_UPDATE = 0x45
	TYPE_AGENT_CONSOLE_OUT = 0x46
	TYPE_AGENT_UPDATE      = 0x47
	TYPE_AGENT_REMOVE      = 0x48

	TYPE_DOWNLOAD_CREATE = 0x51
	TYPE_DOWNLOAD_UPDATE = 0x52
	TYPE_DOWNLOAD_DELETE = 0x53
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

func CreateSpAgentUpdate(agentData AgentData) SyncPackerAgentUpdate {
	return SyncPackerAgentUpdate{
		store:  STORE_LOG,
		SpType: TYPE_AGENT_UPDATE,

		Id:       agentData.Id,
		Sleep:    agentData.Sleep,
		Jitter:   agentData.Jitter,
		Elevated: agentData.Elevated,
		Username: agentData.Username,
		Tags:     agentData.Tags,
	}
}

func CreateSpAgentTick(AgentID string) SyncPackerAgentTick {
	return SyncPackerAgentTick{
		store:  STORE_TICK,
		SpType: TYPE_AGENT_TICK,

		Id: AgentID,
	}
}

func CreateSpAgentConsoleOutput(agentId string, messageType int, message string, text string) SyncPackerAgentConsoleOutput {
	return SyncPackerAgentConsoleOutput{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_CONSOLE_OUT,

		AgentId:     agentId,
		MessageType: messageType,
		Message:     message,
		ClearText:   text,
	}
}

func CreateSpAgentTaskCreate(taskData TaskData) SyncPackerAgentTaskCreate {
	return SyncPackerAgentTaskCreate{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_TASK_CREATE,

		AgentId:   taskData.AgentId,
		TaskId:    taskData.TaskId,
		StartTime: taskData.StartDate,
		CmdLine:   taskData.CommandLine,
		TaskType:  taskData.Type,
		User:      taskData.User,
	}
}

func CreateSpAgentTaskUpdate(taskData TaskData) SyncPackerAgentTaskUpdate {
	return SyncPackerAgentTaskUpdate{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_TASK_UPDATE,

		AgentId:     taskData.AgentId,
		TaskId:      taskData.TaskId,
		TaskType:    taskData.Type,
		FinishTime:  taskData.FinishDate,
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Text:        taskData.ClearText,
		Completed:   taskData.Completed,
	}
}

func CreateSpAgentRemove(agentId string) SyncPackerAgentRemove {
	return SyncPackerAgentRemove{
		store:  STORE_LOG,
		SpType: TYPE_AGENT_REMOVE,

		AgentId: agentId,
	}
}

/// DOWNLOAD

func CreateSpDownloadCreate(downloadData DownloadData) SyncPackerDownloadCreate {
	return SyncPackerDownloadCreate{
		store:  STORE_LOG,
		SpType: TYPE_DOWNLOAD_CREATE,

		AgentId:   downloadData.AgentId,
		AgentName: downloadData.AgentName,
		FileId:    downloadData.FileId,
		Computer:  downloadData.Computer,
		File:      downloadData.RemotePath,
		Size:      downloadData.TotalSize,
		Date:      downloadData.Date,
	}
}

func CreateSpDownloadUpdate(downloadData DownloadData) SyncPackerDownloadUpdate {
	return SyncPackerDownloadUpdate{
		store:  STORE_LOG,
		SpType: TYPE_DOWNLOAD_UPDATE,

		FileId:   downloadData.FileId,
		RecvSize: downloadData.RecvSize,
		State:    downloadData.State,
	}
}

func CreateSpDownloadDelete(fileId string) SyncPackerDownloadDelete {
	return SyncPackerDownloadDelete{
		store:  STORE_LOG,
		SpType: TYPE_DOWNLOAD_DELETE,

		FileId: fileId,
	}
}
