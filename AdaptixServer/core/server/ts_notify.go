package server

import (
	"fmt"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsNotifyClient(connected bool, username string) {
	var packet SpNotification

	if connected {
		message := fmt.Sprintf("Operator '%v' connected to teamserver", username)
		packet = CreateSpNotification(NOTIFY_CLIENT_CONNECT, message)
	} else {
		message := fmt.Sprintf("Operator '%v' disconnected from teamserver", username)
		packet = CreateSpNotification(NOTIFY_CLIENT_DISCONNECT, message)
	}

	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
	ts.notifications.Put(packet)
}

func (ts *Teamserver) TsNotifyListenerStart(restart bool, listenerName string, listenerType string) {
	var message string
	if restart {
		message = fmt.Sprintf("Listener '%v' reconfigured", listenerName)
	} else {
		message = fmt.Sprintf("Listener '%v' (%v) started", listenerName, listenerType)
	}

	packet := CreateSpNotification(NOTIFY_LISTENER_START, message)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
	ts.notifications.Put(packet)
}

func (ts *Teamserver) TsNotifyListenerStop(listenerName string, listenerType string) {
	message := fmt.Sprintf("Listener '%v' stopped", listenerName)
	packet := CreateSpNotification(NOTIFY_LISTENER_STOP, message)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
	ts.notifications.Put(packet)
}

func (ts *Teamserver) TsNotifyAgent(restore bool, agentData adaptix.AgentData) {
	message := "New "
	postMsg := agentData.Computer

	if len(agentData.Domain) != 0 && strings.ToLower(agentData.Computer) != strings.ToLower(agentData.Domain) {
		postMsg += "." + agentData.Domain + "'"
	} else {
		postMsg += "'"
	}
	if len(agentData.InternalIP) != 0 {
		postMsg = postMsg + " (" + agentData.InternalIP + ")"
	}
	if restore {
		message = "Restore "
		createdAt := time.Unix(agentData.CreateTime, 0).Format("15:04:05 02.01.2006")
		postMsg = postMsg + fmt.Sprintf(" [created '%v']", createdAt)
	}

	message += fmt.Sprintf("'%v' (%v) executed on '%v @ %v", agentData.Name, agentData.Id, agentData.Username, postMsg)

	packet := CreateSpNotification(NOTIFY_AGENT_NEW, message)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
	ts.notifications.Put(packet)
}

func (ts *Teamserver) TsNotifyTunnelAdd(tunnel *Tunnel) {
	if tunnel.Data.Client == "" {
		message := ""
		switch tunnel.Type {
		case adaptix.TUNNEL_TYPE_SOCKS4:
			message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_SOCKS5:
			message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_LOCAL_PORT:
			message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_REVERSE:
			message = fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)
		default:
			return
		}

		packet := CreateSpNotification(NOTIFY_TUNNEL_START, message)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
		ts.notifications.Put(packet)
	}
}

func (ts *Teamserver) TsNotifyTunnelRemove(tunnel *Tunnel) {
	if tunnel.Data.Client == "" {
		message := ""
		switch tunnel.Type {
		case adaptix.TUNNEL_TYPE_SOCKS4:
			message = fmt.Sprintf("SOCKS4 server ':%s' stopped", tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_SOCKS5:
			message = fmt.Sprintf("SOCKS5 server ':%s' stopped", tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
			message = fmt.Sprintf("SOCKS5 (with Auth) server ':%s' stopped", tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_LOCAL_PORT:
			message = fmt.Sprintf("Local port forward on ':%s' stopped", tunnel.Data.Port)
		case adaptix.TUNNEL_TYPE_REVERSE:
			message = fmt.Sprintf("Remote port forward to '%s:%s' stopped", tunnel.Data.Fhost, tunnel.Data.Fport)
		default:
			return
		}

		packet := CreateSpNotification(NOTIFY_TUNNEL_STOP, message)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryNotifications)
		ts.notifications.Put(packet)
	}
}
