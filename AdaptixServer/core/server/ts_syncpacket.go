package server

import (
	"time"

	"github.com/Adaptix-Framework/axc2"
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
	TYPE_SYNC_START          = 0x11
	TYPE_SYNC_FINISH         = 0x12
	TYPE_SYNC_BATCH          = 0x14
	TYPE_SYNC_CATEGORY_BATCH = 0x15

	TYPE_CHAT_MESSAGE = 0x18

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
	TYPE_AGENT_TASK_HOOK   = 0x4d

	TYPE_DOWNLOAD_CREATE = 0x51
	TYPE_DOWNLOAD_UPDATE = 0x52
	TYPE_DOWNLOAD_DELETE = 0x53

	TYPE_TUNNEL_CREATE = 0x57
	TYPE_TUNNEL_EDIT   = 0x58
	TYPE_TUNNEL_DELETE = 0x59

	TYPE_SCREEN_CREATE = 0x5b
	TYPE_SCREEN_UPDATE = 0x5c
	TYPE_SCREEN_DELETE = 0x5d

	TYPE_BROWSER_DISKS        = 0x61
	TYPE_BROWSER_FILES        = 0x62
	TYPE_BROWSER_FILES_STATUS = 0x63
	TYPE_BROWSER_PROCESS      = 0x64

	TYPE_AGENT_CONSOLE_OUT       = 0x69
	TYPE_AGENT_CONSOLE_TASK_SYNC = 0x6a
	TYPE_AGENT_CONSOLE_TASK_UPD  = 0x6b

	TYPE_PIVOT_CREATE = 0x71
	TYPE_PIVOT_DELETE = 0x72

	TYPE_CREDS_CREATE  = 0x81
	TYPE_CREDS_EDIT    = 0x82
	TYPE_CREDS_DELETE  = 0x83
	TYPE_CREDS_SET_TAG = 0x84

	TYPE_TARGETS_CREATE  = 0x87
	TYPE_TARGETS_EDIT    = 0x88
	TYPE_TARGETS_DELETE  = 0x89
	TYPE_TARGETS_SET_TAG = 0x8a
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

func CreateSpSyncStart(count int, addrs []string) SyncPackerStart {
	return SyncPackerStart{
		SpType: TYPE_SYNC_START,

		Count:     count,
		Addresses: addrs,
	}
}

func CreateSpSyncFinish() SyncPackerFinish {
	return SyncPackerFinish{
		SpType: TYPE_SYNC_FINISH,
	}
}

func CreateSpSyncBatch(packets []interface{}) SyncPackerBatch {
	return SyncPackerBatch{
		SpType:  TYPE_SYNC_BATCH,
		Packets: packets,
	}
}

func CreateSpSyncCategoryBatch(category string, packets []interface{}) SyncPackerCategoryBatch {
	return SyncPackerCategoryBatch{
		SpType:   TYPE_SYNC_CATEGORY_BATCH,
		Category: category,
		Packets:  packets,
	}
}

/// LISTENER

func CreateSpListenerReg(name string, protocol string, l_type string, ax string) SyncPackerListenerReg {
	return SyncPackerListenerReg{
		SpType: TYPE_LISTENER_REG,

		Name:     name,
		Protocol: protocol,
		Type:     l_type,
		AX:       ax,
	}
}

func CreateSpListenerStart(listenerData adaptix.ListenerData) SyncPackerListenerStart {
	return SyncPackerListenerStart{
		SpType: TYPE_LISTENER_START,

		ListenerName:     listenerData.Name,
		ListenerRegName:  listenerData.RegName,
		ListenerProtocol: listenerData.Protocol,
		ListenerType:     listenerData.Type,
		BindHost:         listenerData.BindHost,
		BindPort:         listenerData.BindPort,
		AgentAddrs:       listenerData.AgentAddr,
		ListenerStatus:   listenerData.Status,
		Data:             listenerData.Data,
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

func CreateSpAgentReg(agent string, ax string, listeners []string) SyncPackerAgentReg {
	return SyncPackerAgentReg{
		SpType: TYPE_AGENT_REG,

		Agent:     agent,
		AX:        ax,
		Listeners: listeners,
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
		WorkingTime:  agentData.WorkingTime,
		KillDate:     agentData.KillDate,
		Sleep:        agentData.Sleep,
		Jitter:       agentData.Jitter,
		ACP:          agentData.ACP,
		OemCP:        agentData.OemCP,
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
		Sleep:        &agentData.Sleep,
		Jitter:       &agentData.Jitter,
		WorkingTime:  &agentData.WorkingTime,
		KillDate:     &agentData.KillDate,
		Impersonated: &agentData.Impersonated,
		Tags:         &agentData.Tags,
		Mark:         &agentData.Mark,
		Color:        &agentData.Color,
		InternalIP:   &agentData.InternalIP,
		ExternalIP:   &agentData.ExternalIP,
		GmtOffset:    &agentData.GmtOffset,
		ACP:          &agentData.ACP,
		OemCP:        &agentData.OemCP,
		Pid:          &agentData.Pid,
		Tid:          &agentData.Tid,
		Arch:         &agentData.Arch,
		Elevated:     &agentData.Elevated,
		Process:      &agentData.Process,
		Os:           &agentData.Os,
		OsDesc:       &agentData.OsDesc,
		Domain:       &agentData.Domain,
		Computer:     &agentData.Computer,
		Username:     &agentData.Username,
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

func CreateSpAgentTaskHook(taskData adaptix.TaskData, jobIndex int) SyncPackerAgentTaskHook {
	return SyncPackerAgentTaskHook{
		SpType: TYPE_AGENT_TASK_HOOK,

		AgentId:     taskData.AgentId,
		TaskId:      taskData.TaskId,
		HookId:      taskData.HookId,
		JobIndex:    jobIndex,
		MessageType: taskData.MessageType,
		Message:     taskData.Message,
		Text:        taskData.ClearText,
		Completed:   taskData.Completed,
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

/// CHAT

func CreateSpChatMessage(chatData adaptix.ChatData) SyncPackerChatMessage {
	return SyncPackerChatMessage{
		SpType: TYPE_CHAT_MESSAGE,

		Username: chatData.Username,
		Message:  chatData.Message,
		Date:     chatData.Date,
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

func CreateSpDownloadDelete(fileId []string) SyncPackerDownloadDelete {
	return SyncPackerDownloadDelete{
		SpType: TYPE_DOWNLOAD_DELETE,

		FileId: fileId,
	}
}

/// SCREEN

func CreateSpScreenshotCreate(screenData adaptix.ScreenData) SyncPackerScreenshotCreate {
	return SyncPackerScreenshotCreate{
		SpType: TYPE_SCREEN_CREATE,

		ScreenId: screenData.ScreenId,
		User:     screenData.User,
		Computer: screenData.Computer,
		Note:     screenData.Note,
		Date:     screenData.Date,
		Content:  screenData.Content,
	}
}

func CreateSpScreenshotUpdate(screenId string, note string) SyncPackerScreenshotUpdate {
	return SyncPackerScreenshotUpdate{
		SpType: TYPE_SCREEN_UPDATE,

		ScreenId: screenId,
		Note:     note,
	}
}

func CreateSpScreenshotDelete(screenId string) SyncPackerScreenshotDelete {
	return SyncPackerScreenshotDelete{
		SpType: TYPE_SCREEN_DELETE,

		ScreenId: screenId,
	}
}

/// CREDS

func CreateSpCredentialsAdd(creds []*adaptix.CredsData) SyncPackerCredentialsAdd {
	var syncCreds []SyncPackerCredentials

	for _, credsData := range creds {
		t := SyncPackerCredentials{
			CredId:   credsData.CredId,
			Username: credsData.Username,
			Password: credsData.Password,
			Realm:    credsData.Realm,
			Type:     credsData.Type,
			Tag:      credsData.Tag,
			Date:     credsData.Date,
			Storage:  credsData.Storage,
			AgentId:  credsData.AgentId,
			Host:     credsData.Host,
		}
		syncCreds = append(syncCreds, t)
	}

	return SyncPackerCredentialsAdd{
		SpType: TYPE_CREDS_CREATE,
		Creds:  syncCreds,
	}
}

func CreateSpCredentialsUpdate(credsData adaptix.CredsData) SyncPackerCredentialsUpdate {
	return SyncPackerCredentialsUpdate{
		SpType: TYPE_CREDS_EDIT,

		CredId:   credsData.CredId,
		Username: credsData.Username,
		Password: credsData.Password,
		Realm:    credsData.Realm,
		Type:     credsData.Type,
		Tag:      credsData.Tag,
		Storage:  credsData.Storage,
		Host:     credsData.Host,
	}
}

func CreateSpCredentialsDelete(credsId []string) SyncPackerCredentialsDelete {
	return SyncPackerCredentialsDelete{
		SpType: TYPE_CREDS_DELETE,

		CredsId: credsId,
	}
}

func CreateSpCredentialsSetTag(credsId []string, tag string) SyncPackerCredentialsTag {
	return SyncPackerCredentialsTag{
		SpType: TYPE_CREDS_SET_TAG,

		CredsId: credsId,
		Tag:     tag,
	}
}

/// TARGETS

func CreateSpTargetsAdd(targetsData []*adaptix.TargetData) SyncPackerTargetsAdd {
	var syncTargets []SyncPackerTarget

	for _, targetData := range targetsData {
		t := SyncPackerTarget{
			TargetId: targetData.TargetId,
			Computer: targetData.Computer,
			Domain:   targetData.Domain,
			Address:  targetData.Address,
			Os:       targetData.Os,
			OsDesk:   targetData.OsDesk,
			Tag:      targetData.Tag,
			Info:     targetData.Info,
			Date:     targetData.Date,
			Alive:    targetData.Alive,
			Agents:   targetData.Agents,
		}
		syncTargets = append(syncTargets, t)
	}

	return SyncPackerTargetsAdd{
		SpType:  TYPE_TARGETS_CREATE,
		Targets: syncTargets,
	}
}

func CreateSpTargetUpdate(targetData adaptix.TargetData) SyncPackerTargetUpdate {
	return SyncPackerTargetUpdate{
		SpType: TYPE_TARGETS_EDIT,

		TargetId: targetData.TargetId,
		Computer: targetData.Computer,
		Domain:   targetData.Domain,
		Address:  targetData.Address,
		Os:       targetData.Os,
		OsDesk:   targetData.OsDesk,
		Tag:      targetData.Tag,
		Info:     targetData.Info,
		Date:     targetData.Date,
		Alive:    targetData.Alive,
		Agents:   targetData.Agents,
	}
}

func CreateSpTargetDelete(targetsId []string) SyncPackerTargetDelete {
	return SyncPackerTargetDelete{
		SpType: TYPE_TARGETS_DELETE,

		TargetsId: targetsId,
	}
}

func CreateSpTargetSetTag(targetsId []string, tag string) SyncPackerTargetTag {
	return SyncPackerTargetTag{
		SpType: TYPE_TARGETS_SET_TAG,

		TargetsId: targetsId,
		Tag:       tag,
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
