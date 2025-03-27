package server

import (
	"AdaptixServer/core/adaptix"
	"time"
)

const (
	SP_TYPE_EVENT = 0x13
)

const (
	EVENT_CLIENT_CONNECT    = 1
	EVENT_CLIENT_DISCONNECT = 2
	EVENT_LISTENER_START    = 3
	EVENT_LISTENER_STOP     = 4
	EVENT_AGENT_NEW         = 5
	EVENT_TUNNEL_START      = 6
	EVENT_TUNNEL_STOP       = 7
)

const (
	TYPE_SYNC_START  = 0x11
	TYPE_SYNC_FINISH = 0x12

	TYPE_LISTENER_REG   = 0x31
	TYPE_LISTENER_START = 0x32
	TYPE_LISTENER_STOP  = 0x33
	TYPE_LISTENER_EDIT  = 0x34

	TYPE_AGENT_REG    = 0x41
	TYPE_AGENT_NEW    = 0x42
	TYPE_AGENT_TICK   = 0x43
	TYPE_AGENT_UPDATE = 0x44
	TYPE_AGENT_LINK   = 0x45
	TYPE_AGENT_REMOVE = 0x46

	TYPE_AGENT_TASK_SYNC   = 0x49
	TYPE_AGENT_TASK_UPDATE = 0x4a
	TYPE_AGENT_TASK_SEND   = 0x4b
	TYPE_AGENT_TASK_REMOVE = 0x4c

	TYPE_DOWNLOAD_CREATE = 0x51
	TYPE_DOWNLOAD_UPDATE = 0x52
	TYPE_DOWNLOAD_DELETE = 0x53

	TYPE_TUNNEL_CREATE = 0x57
	TYPE_TUNNEL_EDIT   = 0x58
	TYPE_TUNNEL_DELETE = 0x59

	TYPE_BROWSER_DISKS        = 0x61
	TYPE_BROWSER_FILES        = 0x62
	TYPE_BROWSER_FILES_STATUS = 0x63
	TYPE_BROWSER_PROCESS      = 0x64

	TYPE_AGENT_CONSOLE_OUT       = 0x69
	TYPE_AGENT_CONSOLE_TASK_SYNC = 0x6a
	TYPE_AGENT_CONSOLE_TASK_UPD  = 0x6b

	TYPE_PIVOT_CREATE = 0x71
	TYPE_PIVOT_DELETE = 0x72
)

func CreateSpEvent(event int, message string) SpEvent {
	return SpEvent{
		Type: SP_TYPE_EVENT,

		EventType: event,
		Message:   message,
		Date:      time.Now().UTC().Unix(),
	}
}

////////////////////////////////////////////////////////////////////////////////////////////

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

/// LISTENER

func CreateSpListenerReg(fn string, ui string) SyncPackerListenerReg {
	return SyncPackerListenerReg{
		SpType: TYPE_LISTENER_REG,

		ListenerFN: fn,
		ListenerUI: ui,
	}
}

func CreateSpListenerStart(listenerData adaptix.ListenerData) SyncPackerListenerStart {
	return SyncPackerListenerStart{
		SpType: TYPE_LISTENER_START,

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

func CreateSpListenerEdit(listenerData adaptix.ListenerData) SyncPackerListenerStart {
	packet := CreateSpListenerStart(listenerData)
	packet.SpType = TYPE_LISTENER_EDIT
	return packet
}

func CreateSpListenerStop(name string) SyncPackerListenerStop {
	return SyncPackerListenerStop{
		SpType: TYPE_LISTENER_STOP,

		ListenerName: name,
	}
}

/// AGENT

func CreateSpAgentReg(agent string, listeners []string, ui string, cmd string) SyncPackerAgentReg {
	return SyncPackerAgentReg{
		SpType: TYPE_AGENT_REG,

		Agent:     agent,
		Listeners: listeners,
		AgentUI:   ui,
		AgentCmd:  cmd,
	}
}

func CreateSpAgentNew(agentData adaptix.AgentData) SyncPackerAgentNew {
	return SyncPackerAgentNew{
		SpType: TYPE_AGENT_NEW,

		Id:           agentData.Id,
		Name:         agentData.Name,
		Listener:     agentData.Listener,
		Async:        agentData.Async,
		ExternalIP:   agentData.ExternalIP,
		InternalIP:   agentData.InternalIP,
		GmtOffset:    agentData.GmtOffset,
		Sleep:        agentData.Sleep,
		Jitter:       agentData.Jitter,
		Pid:          agentData.Pid,
		Tid:          agentData.Tid,
		Arch:         agentData.Arch,
		Elevated:     agentData.Elevated,
		Process:      agentData.Process,
		Os:           agentData.Os,
		OsDesc:       agentData.OsDesc,
		Domain:       agentData.Domain,
		Computer:     agentData.Computer,
		Username:     agentData.Username,
		Impersonated: agentData.Impersonated,
		LastTick:     agentData.LastTick,
		Tags:         agentData.Tags,
		Mark:         agentData.Mark,
		Color:        agentData.Color,
	}
}

func CreateSpAgentUpdate(agentData adaptix.AgentData) SyncPackerAgentUpdate {
	return SyncPackerAgentUpdate{
		SpType: TYPE_AGENT_UPDATE,

		Id:           agentData.Id,
		Sleep:        agentData.Sleep,
		Jitter:       agentData.Jitter,
		Impersonated: agentData.Impersonated,
		Tags:         agentData.Tags,
		Mark:         agentData.Mark,
		Color:        agentData.Color,
	}
}

func CreateSpAgentTick(agents []string) SyncPackerAgentTick {
	return SyncPackerAgentTick{
		SpType: TYPE_AGENT_TICK,

		Id: agents,
	}
}

func CreateSpAgentRemove(agentId string) SyncPackerAgentRemove {
	return SyncPackerAgentRemove{
		SpType: TYPE_AGENT_REMOVE,

		AgentId: agentId,
	}
}

func CreateSpAgentTaskSync(taskData adaptix.TaskData) SyncPackerAgentTaskSync {
	return SyncPackerAgentTaskSync{
		SpType: TYPE_AGENT_TASK_SYNC,

		AgentId:     taskData.AgentId,
		TaskId:      taskData.TaskId,
		StartTime:   taskData.StartDate,
		CmdLine:     taskData.CommandLine,
		TaskType:    taskData.Type,
		Client:      taskData.Client,
		User:        taskData.User,
		Computer:    taskData.Computer,
		FinishTime:  taskData.FinishDate,
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Text:        taskData.ClearText,
		Completed:   taskData.Completed,
	}
}

func CreateSpAgentTaskUpdate(taskData adaptix.TaskData) SyncPackerAgentTaskUpdate {
	return SyncPackerAgentTaskUpdate{
		SpType: TYPE_AGENT_TASK_UPDATE,

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

func CreateSpAgentTaskSend(tasksId []string) SyncPackerAgentTaskSend {
	return SyncPackerAgentTaskSend{
		SpType: TYPE_AGENT_TASK_SEND,

		TaskId: tasksId,
	}
}

func CreateSpAgentTaskRemove(taskData adaptix.TaskData) SyncPackerAgentTaskRemove {
	return SyncPackerAgentTaskRemove{
		SpType: TYPE_AGENT_TASK_REMOVE,

		TaskId: taskData.TaskId,
	}
}

func CreateSpAgentConsoleOutput(agentId string, messageType int, message string, text string) SyncPackerAgentConsoleOutput {
	return SyncPackerAgentConsoleOutput{
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_CONSOLE_OUT,

		AgentId:     agentId,
		MessageType: messageType,
		Message:     message,
		ClearText:   text,
	}
}

func CreateSpAgentConsoleTaskSync(taskData adaptix.TaskData) SyncPackerAgentConsoleTaskSync {
	return SyncPackerAgentConsoleTaskSync{
		SpType: TYPE_AGENT_CONSOLE_TASK_SYNC,

		AgentId:     taskData.AgentId,
		TaskId:      taskData.TaskId,
		StartTime:   taskData.StartDate,
		CmdLine:     taskData.CommandLine,
		Client:      taskData.Client,
		FinishTime:  taskData.FinishDate,
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Text:        taskData.ClearText,
		Completed:   taskData.Completed,
	}
}

func CreateSpAgentConsoleTaskUpd(taskData adaptix.TaskData) SyncPackerAgentConsoleTaskUpd {
	return SyncPackerAgentConsoleTaskUpd{
		SpType: TYPE_AGENT_CONSOLE_TASK_UPD,

		AgentId:     taskData.AgentId,
		TaskId:      taskData.TaskId,
		FinishTime:  taskData.FinishDate,
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Text:        taskData.ClearText,
		Completed:   taskData.Completed,
	}
}

/// PIVOT

func CreateSpPivotCreate(pivotData adaptix.PivotData) SyncPackerPivotCreate {
	return SyncPackerPivotCreate{
		SpType: TYPE_PIVOT_CREATE,

		PivotId:       pivotData.PivotId,
		PivotName:     pivotData.PivotName,
		ParentAgentId: pivotData.ParentAgentId,
		ChildAgentId:  pivotData.ChildAgentId,
	}
}

func CreateSpPivotDelete(pivotId string) SyncPackerPivotDelete {
	return SyncPackerPivotDelete{
		SpType: TYPE_PIVOT_DELETE,

		PivotId: pivotId,
	}
}

/// DOWNLOAD

func CreateSpDownloadCreate(downloadData adaptix.DownloadData) SyncPackerDownloadCreate {
	return SyncPackerDownloadCreate{
		SpType: TYPE_DOWNLOAD_CREATE,

		AgentId:   downloadData.AgentId,
		AgentName: downloadData.AgentName,
		FileId:    downloadData.FileId,
		User:      downloadData.User,
		Computer:  downloadData.Computer,
		File:      downloadData.RemotePath,
		Size:      downloadData.TotalSize,
		Date:      downloadData.Date,
	}
}

func CreateSpDownloadUpdate(downloadData adaptix.DownloadData) SyncPackerDownloadUpdate {
	return SyncPackerDownloadUpdate{
		SpType: TYPE_DOWNLOAD_UPDATE,

		FileId:   downloadData.FileId,
		RecvSize: downloadData.RecvSize,
		State:    downloadData.State,
	}
}

func CreateSpDownloadDelete(fileId string) SyncPackerDownloadDelete {
	return SyncPackerDownloadDelete{
		SpType: TYPE_DOWNLOAD_DELETE,

		FileId: fileId,
	}
}

/// BROWSER

func CreateSpBrowserDisks(taskData adaptix.TaskData, data string) SyncPacketBrowserDisks {
	return SyncPacketBrowserDisks{
		SpType: TYPE_BROWSER_DISKS,

		AgentId:     taskData.AgentId,
		Time:        time.Now().UTC().Unix(),
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Data:        data,
	}
}

func CreateSpBrowserFiles(taskData adaptix.TaskData, path string, data string) SyncPacketBrowserFiles {
	return SyncPacketBrowserFiles{
		SpType: TYPE_BROWSER_FILES,

		AgentId:     taskData.AgentId,
		Time:        time.Now().UTC().Unix(),
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Path:        path,
		Data:        data,
	}
}

func CreateSpBrowserFilesStatus(taskData adaptix.TaskData) SyncPacketBrowserFilesStatus {
	return SyncPacketBrowserFilesStatus{
		SpType: TYPE_BROWSER_FILES_STATUS,

		AgentId:     taskData.AgentId,
		Time:        time.Now().UTC().Unix(),
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
	}
}

func CreateSpBrowserProcess(taskData adaptix.TaskData, data string) SyncPacketBrowserProcess {
	return SyncPacketBrowserProcess{
		SpType: TYPE_BROWSER_PROCESS,

		AgentId:     taskData.AgentId,
		Time:        time.Now().UTC().Unix(),
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Data:        data,
	}
}

/// TUNNEL

func CreateSpTunnelCreate(tunnelData adaptix.TunnelData) SyncPackerTunnelCreate {
	return SyncPackerTunnelCreate{
		SpType: TYPE_TUNNEL_CREATE,

		TunnelId:  tunnelData.TunnelId,
		AgentId:   tunnelData.AgentId,
		Username:  tunnelData.Username,
		Computer:  tunnelData.Computer,
		Process:   tunnelData.Process,
		Type:      tunnelData.Type,
		Info:      tunnelData.Info,
		Interface: tunnelData.Interface,
		Port:      tunnelData.Port,
		Client:    tunnelData.Client,
		Fport:     tunnelData.Fport,
		Fhost:     tunnelData.Fhost,
	}
}

func CreateSpTunnelEdit(tunnelData adaptix.TunnelData) SyncPackerTunnelEdit {
	return SyncPackerTunnelEdit{
		SpType: TYPE_TUNNEL_EDIT,

		TunnelId: tunnelData.TunnelId,
		Info:     tunnelData.Info,
	}
}

func CreateSpTunnelDelete(tunnelData adaptix.TunnelData) SyncPackerTunnelDelete {
	return SyncPackerTunnelDelete{
		SpType: TYPE_TUNNEL_DELETE,

		TunnelId: tunnelData.TunnelId,
	}
}
