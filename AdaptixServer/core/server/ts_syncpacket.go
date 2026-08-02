package server

import (
	"time"

	"AdaptixServer/core/database"

	"github.com/Adaptix-Framework/axc2/v2"
)

const (
	SP_TYPE_NOTIFICATION = 0x13
)

const (
	NOTIFY_CLIENT_CONNECT    = 1
	NOTIFY_CLIENT_DISCONNECT = 2
	NOTIFY_LISTENER_START    = 3
	NOTIFY_LISTENER_STOP     = 4
	NOTIFY_AGENT_NEW         = 5
	NOTIFY_TUNNEL_START      = 6
	NOTIFY_TUNNEL_STOP       = 7
)

const (
	TYPE_SYNC_START          = 0x11
	TYPE_SYNC_FINISH         = 0x12
	TYPE_SYNC_BATCH          = 0x14
	TYPE_SYNC_CATEGORY_BATCH = 0x15
	TYPE_LOG_BATCH           = 0x16

	TYPE_CHAT_MESSAGE       = 0x18
	TYPE_CHAT_EDIT          = 0x19
	TYPE_CHAT_DELETE        = 0x1a
	TYPE_CHAT_REACTION      = 0x1b
	TYPE_CHAT_TODO          = 0x1c
	TYPE_CHAT_SEARCH_RESULT = 0x20

	TYPE_PLUGIN_SERVICE_DATA  = 0x1d
	TYPE_PLUGIN_AGENT_DATA    = 0x1e
	TYPE_PLUGIN_LISTENER_DATA = 0x1f

	TYPE_LISTENER_REG = 0x21
	TYPE_AGENT_REG    = 0x22
	TYPE_SERVICE_REG  = 0x23

	TYPE_LISTENER_START = 0x31
	TYPE_LISTENER_EDIT  = 0x32
	TYPE_LISTENER_STOP  = 0x33

	TYPE_AGENT_NEW    = 0x41
	TYPE_AGENT_UPDATE = 0x42
	TYPE_AGENT_REMOVE = 0x43
	TYPE_AGENT_TICK   = 0x44
	TYPE_AGENT_LINK   = 0x45

	TYPE_AGENT_TASK_SYNC   = 0x49
	TYPE_AGENT_TASK_UPDATE = 0x4a
	TYPE_AGENT_TASK_SEND   = 0x4b
	TYPE_AGENT_TASK_REMOVE = 0x4c
	TYPE_AGENT_TASK_HOOK   = 0x4d

	TYPE_TRANSFER_CREATE  = 0x51
	TYPE_TRANSFER_UPDATE  = 0x52
	TYPE_TRANSFER_DELETE  = 0x53
	TYPE_TRANSFER_ACTUAL  = 0x54
	TYPE_TRANSFER_SET_TAG = 0x55

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

	TYPE_AGENT_CONSOLE_LOCAL     = 0x67
	TYPE_AGENT_CONSOLE_ERROR     = 0x68
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

	TYPE_AXSCRIPT_COMMANDS = 0x91
	TYPE_AXSCRIPT_LIST     = 0x92
	TYPE_EVENT_HANDLERS    = 0x93

	TYPE_GROUP_CREATE   = 0xa1
	TYPE_GROUP_RENAME   = 0xa2
	TYPE_GROUP_DELETE   = 0xa3
	TYPE_GROUP_MEMBERS  = 0xa4
	TYPE_GROUP_REPARENT = 0xa5

	TYPE_PAYLOAD_CREATE = 0xb1
	TYPE_PAYLOAD_UPDATE = 0xb2
	TYPE_PAYLOAD_DELETE = 0xb3
	TYPE_PAYLOAD_EDIT   = 0xb4
)

func CreateSpNotification(notifyType int, message string) SpNotification {
	return SpNotification{
		Type: SP_TYPE_NOTIFICATION,

		NotifyType: notifyType,
		Message:    message,
		Date:       time.Now().UTC().Unix(),
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
		CreateTime:       listenerData.CreateTime,
		ListenerStatus:   listenerData.Status,
		Tags:             listenerData.Tags,
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

func CreateSpAgentReg(agent string, ax string, listeners []string, multiListeners bool, groups []AxCommandBatch) SyncPackerAgentReg {
	return SyncPackerAgentReg{
		SpType: TYPE_AGENT_REG,

		Agent:          agent,
		AX:             ax,
		Listeners:      listeners,
		MultiListeners: multiListeners,
		Groups:         groups,
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
		CreateTime:   agentData.CreateTime,
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
		Listener:     &agentData.Listener,
	}
}

func CreateSpAgentTick(agents []int64) SyncPackerAgentTick {
	return SyncPackerAgentTick{
		SpType: TYPE_AGENT_TICK,

		Id: agents,
	}
}

func CreateSpAgentRemove(agentId int64) SyncPackerAgentRemove {
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

func CreateSpAgentTaskSend(tasksId []int64) SyncPackerAgentTaskSend {
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

func CreateSpAgentConsoleOutput(agentId int64, messageType int, message string, text string) SyncPackerAgentConsoleOutput {
	return SyncPackerAgentConsoleOutput{
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_CONSOLE_OUT,

		AgentId:     agentId,
		MessageType: messageType,
		Message:     message,
		ClearText:   text,
	}
}

func CreateSpAgentErrorCommand(agentId int64, cmdline string, message string, HookId string, HandlerId string) SyncPackerAgentErrorCommand {
	return SyncPackerAgentErrorCommand{
		SpType: TYPE_AGENT_CONSOLE_ERROR,

		AgentId:   agentId,
		Cmdline:   cmdline,
		Message:   message,
		HookId:    HookId,
		HandlerId: HandlerId,
	}
}

func CreateSpAgentLocalCommand(agentId int64, cmdline string, message string, text string) SyncPackerAgentLocalCommand {
	return SyncPackerAgentLocalCommand{
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_AGENT_CONSOLE_LOCAL,

		AgentId: agentId,
		Cmdline: cmdline,
		Message: message,
		Text:    text,
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

func CreateSpChatMessageEx(data database.ChatDataEx) SyncPackerChatMessage {
	return SyncPackerChatMessage{
		SpType:      TYPE_CHAT_MESSAGE,
		Id:          data.Id,
		Username:    data.Username,
		Message:     data.Message,
		Date:        data.Date,
		Edited:      data.Edited,
		Deleted:     data.Deleted,
		DeletedDate: data.DeletedDate,
		Reactions:   data.Reactions,
		ReplyToId:   data.ReplyToId,
		ReplyToName: data.ReplyToName,
	}
}

func CreateSpChatEdit(id int64, newMessage string) SyncPackerChatEdit {
	return SyncPackerChatEdit{
		SpType:  TYPE_CHAT_EDIT,
		Id:      id,
		Message: newMessage,
	}
}

func CreateSpChatDelete(id int64) SyncPackerChatDelete {
	return SyncPackerChatDelete{
		SpType: TYPE_CHAT_DELETE,
		Id:     id,
	}
}

func CreateSpChatReaction(id int64, reactions string) SyncPackerChatReaction {
	return SyncPackerChatReaction{
		SpType:    TYPE_CHAT_REACTION,
		Id:        id,
		Reactions: reactions,
	}
}

func CreateSpChatTodo(content string, updatedBy string, updatedAt int64) SyncPackerChatTodo {
	return SyncPackerChatTodo{
		SpType:    TYPE_CHAT_TODO,
		Content:   content,
		UpdatedBy: updatedBy,
		UpdatedAt: updatedAt,
	}
}

/// TRANSFER (download / upload)

func CreateSpTransferCreate(transferData adaptix.TransferData, transferType int) SyncPackerTransferCreate {
	return SyncPackerTransferCreate{
		SpType:       TYPE_TRANSFER_CREATE,
		TransferType: transferType,

		AgentId:      transferData.AgentId,
		AgentName:    transferData.AgentName,
		FileId:       transferData.FileId,
		User:         transferData.User,
		Computer:     transferData.Computer,
		File:         transferData.RemotePath,
		Size:         transferData.TotalSize,
		Date:         transferData.Date,
		Tag:          transferData.Tag,
		Cancellable:  transferData.Cancellable,
		Kind:         transferData.Kind,
		ArtifactName: transferData.ArtifactName,
		ArtifactType: transferData.ArtifactType,
	}
}

func CreateSpTransferUpdate(transferData adaptix.TransferData, transferType int) SyncPackerTransferUpdate {
	return SyncPackerTransferUpdate{
		SpType:       TYPE_TRANSFER_UPDATE,
		TransferType: transferType,

		FileId:   transferData.FileId,
		Progress: transferData.Progress,
		State:    transferData.State,
	}
}

func CreateSpTransferDelete(fileId []int64, transferType int) SyncPackerTransferDelete {
	return SyncPackerTransferDelete{
		SpType:       TYPE_TRANSFER_DELETE,
		TransferType: transferType,

		FileId: fileId,
	}
}

func CreateSpTransferActual(transferData adaptix.TransferData, transferType int) SyncPackerTransferActual {
	return SyncPackerTransferActual{
		SpType:       TYPE_TRANSFER_ACTUAL,
		TransferType: transferType,

		AgentId:      transferData.AgentId,
		AgentName:    transferData.AgentName,
		FileId:       transferData.FileId,
		User:         transferData.User,
		Computer:     transferData.Computer,
		File:         transferData.RemotePath,
		Size:         transferData.TotalSize,
		Date:         transferData.Date,
		Progress:     transferData.Progress,
		State:        transferData.State,
		Tag:          transferData.Tag,
		Cancellable:  transferData.Cancellable,
		Kind:         transferData.Kind,
		ArtifactName: transferData.ArtifactName,
		ArtifactType: transferData.ArtifactType,
	}
}

func CreateSpTransferSetTag(fileIds []int64, tag string, transferType int) SyncPackerTransferTag {
	return SyncPackerTransferTag{
		SpType:       TYPE_TRANSFER_SET_TAG,
		TransferType: transferType,

		FileId: fileIds,
		Tag:    tag,
	}
}

/// SCREEN

func CreateSpScreenshotCreate(screenData adaptix.ScreenData) SyncPackerScreenshotCreate {
	return SyncPackerScreenshotCreate{
		SpType: TYPE_SCREEN_CREATE,

		ScreenId: screenData.ScreenId,
		AgentId:  screenData.AgentId,
		User:     screenData.User,
		Computer: screenData.Computer,
		Note:     screenData.Note,
		Date:     screenData.Date,
		Content:  screenData.Content,
	}
}

func CreateSpScreenshotUpdate(screenId int64, note string) SyncPackerScreenshotUpdate {
	return SyncPackerScreenshotUpdate{
		SpType: TYPE_SCREEN_UPDATE,

		ScreenId: screenId,
		Note:     note,
	}
}

func CreateSpScreenshotDelete(screenId int64) SyncPackerScreenshotDelete {
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

func CreateSpCredentialsDelete(credsId []int64) SyncPackerCredentialsDelete {
	return SyncPackerCredentialsDelete{
		SpType: TYPE_CREDS_DELETE,

		CredsId: credsId,
	}
}

func CreateSpCredentialsSetTag(credsId []int64, tag string) SyncPackerCredentialsTag {
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

func CreateSpTargetDelete(targetsId []int64) SyncPackerTargetDelete {
	return SyncPackerTargetDelete{
		SpType: TYPE_TARGETS_DELETE,

		TargetsId: targetsId,
	}
}

func CreateSpTargetSetTag(targetsId []int64, tag string) SyncPackerTargetTag {
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

func CreateSpTunnelCreate(tunnelData adaptix.TunnelData, bytesSent, bytesRecv int64) SyncPackerTunnelCreate {
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
		Date:      tunnelData.Date,
		BytesSent: bytesSent,
		BytesRecv: bytesRecv,
		Active:    tunnelData.Active,
	}
}

func CreateSpTunnelEdit(tunnelData adaptix.TunnelData) SyncPackerTunnelEdit {
	return SyncPackerTunnelEdit{
		SpType: TYPE_TUNNEL_EDIT,

		TunnelId: tunnelData.TunnelId,
		Info:     tunnelData.Info,
		Active:   tunnelData.Active,
	}
}

func CreateSpTunnelDelete(tunnelData adaptix.TunnelData) SyncPackerTunnelDelete {
	return SyncPackerTunnelDelete{
		SpType: TYPE_TUNNEL_DELETE,

		TunnelId: tunnelData.TunnelId,
	}
}

/// SERVICE

func CreateSpServiceReg(name string, ax string) SyncPackerServiceReg {
	return SyncPackerServiceReg{
		SpType: TYPE_SERVICE_REG,

		Name: name,
		AX:   ax,
	}
}

func CreateSpPluginServiceData(service string, data string) SyncPackerPluginServiceData {
	return SyncPackerPluginServiceData{
		SpType: TYPE_PLUGIN_SERVICE_DATA,

		Service: service,
		Data:    data,
	}
}

func CreateSpPluginAgentData(agentId int64, agentType string, data string) SyncPackerPluginAgentData {
	return SyncPackerPluginAgentData{
		SpType:    TYPE_PLUGIN_AGENT_DATA,
		AgentId:   agentId,
		AgentType: agentType,
		Data:      data,
	}
}

func CreateSpPluginListenerData(listenerName string, listenerType string, data string) SyncPackerPluginListenerData {
	return SyncPackerPluginListenerData{
		SpType:       TYPE_PLUGIN_LISTENER_DATA,
		Listener:     listenerName,
		ListenerType: listenerType,
		Data:         data,
	}
}

/// LOGS

func CreateSpLogBatch(items []adaptix.LogEntry) SyncPackerLogBatch {
	return SyncPackerLogBatch{
		SpType: TYPE_LOG_BATCH,
		Items:  items,
	}
}

/// GROUPS

func CreateSpGroupCreate(groupId int64, parentId int64, name string, scope string, members []int64) SyncPackerGroupCreate {
	if members == nil {
		members = []int64{}
	}
	return SyncPackerGroupCreate{
		SpType:        TYPE_GROUP_CREATE,
		GroupId:       groupId,
		ParentGroupId: parentId,
		GroupName:     name,
		Scope:         scope,
		Members:       members,
	}
}

func CreateSpGroupRename(groupId int64, name string) SyncPackerGroupRename {
	return SyncPackerGroupRename{
		SpType:    TYPE_GROUP_RENAME,
		GroupId:   groupId,
		GroupName: name,
	}
}

func CreateSpGroupDelete(groupId int64) SyncPackerGroupDelete {
	return SyncPackerGroupDelete{
		SpType:  TYPE_GROUP_DELETE,
		GroupId: groupId,
	}
}

func CreateSpGroupMembers(groupId int64, add []int64, remove []int64) SyncPackerGroupMembers {
	if add == nil {
		add = []int64{}
	}
	if remove == nil {
		remove = []int64{}
	}
	return SyncPackerGroupMembers{
		SpType:  TYPE_GROUP_MEMBERS,
		GroupId: groupId,
		Add:     add,
		Remove:  remove,
	}
}

func CreateSpGroupReparent(groupId int64, newParentId int64) SyncPackerGroupReparent {
	return SyncPackerGroupReparent{
		SpType:      TYPE_GROUP_REPARENT,
		GroupId:     groupId,
		NewParentId: newParentId,
	}
}

/// AXSCRIPT

func CreateSpAxScriptList(items []map[string]interface{}) SyncPackerAxScriptList {
	if items == nil {
		items = []map[string]interface{}{}
	}
	return SyncPackerAxScriptList{
		SpType: TYPE_AXSCRIPT_LIST,
		Items:  items,
	}
}

func CreateSpEventHandlers(items []map[string]interface{}) SyncPackerEventHandlers {
	if items == nil {
		items = []map[string]interface{}{}
	}
	return SyncPackerEventHandlers{
		SpType: TYPE_EVENT_HANDLERS,
		Items:  items,
	}
}

func CreateSpAxScriptData(name string, content string, groups []AxCommandBatch) SyncPackerAxScriptData {
	return SyncPackerAxScriptData{
		SpType:  TYPE_AXSCRIPT_COMMANDS,
		Name:    name,
		Content: content,
		Groups:  groups,
	}
}

/// PAYLOADS

func CreateSpPayloadCreate(p adaptix.PayloadData) SyncPackerPayloadCreate {
	listeners := p.Listeners
	if listeners == nil {
		listeners = []string{}
	}
	return SyncPackerPayloadCreate{
		SpType:    TYPE_PAYLOAD_CREATE,
		PayloadId: p.PayloadId,
		Name:      p.Name,
		AgentType: p.AgentType,
		Artifact:  p.Artifact,
		Arch:      p.Arch,
		Listeners: listeners,
		Size:      p.Size,
		Sha1:      p.Sha1,
		Sha256:    p.Sha256,
		Md5:       p.Md5,
		Creator:   p.Creator,
		Created:   p.Created,
		Hidden:    p.Hidden,
		Filename:  p.Filename,
		BuildId:   p.BuildId,
		Watermark: p.Watermark,
		Notes:     p.Notes,
		Uid:       p.Uid,
		Color:     p.Color,
		Missing:   p.Missing,
	}
}

func CreateSpPayloadUpdate(ids []int64, hidden bool) SyncPackerPayloadUpdate {
	return SyncPackerPayloadUpdate{
		SpType:     TYPE_PAYLOAD_UPDATE,
		PayloadIds: ids,
		Hidden:     hidden,
	}
}

func CreateSpPayloadDelete(ids []int64) SyncPackerPayloadDelete {
	return SyncPackerPayloadDelete{
		SpType:     TYPE_PAYLOAD_DELETE,
		PayloadIds: ids,
	}
}

func CreateSpPayloadEdit(p adaptix.PayloadData) SyncPackerPayloadEdit {
	listeners := p.Listeners
	if listeners == nil {
		listeners = []string{}
	}
	return SyncPackerPayloadEdit{
		SpType:    TYPE_PAYLOAD_EDIT,
		PayloadId: p.PayloadId,
		Name:      p.Name,
		AgentType: p.AgentType,
		Artifact:  p.Artifact,
		Arch:      p.Arch,
		Listeners: listeners,
		Size:      p.Size,
		Sha1:      p.Sha1,
		Sha256:    p.Sha256,
		Md5:       p.Md5,
		Creator:   p.Creator,
		Created:   p.Created,
		Hidden:    p.Hidden,
		Filename:  p.Filename,
		BuildId:   p.BuildId,
		Watermark: p.Watermark,
		Notes:     p.Notes,
		Uid:       p.Uid,
		Color:     p.Color,
		Missing:   p.Missing,
	}
}
