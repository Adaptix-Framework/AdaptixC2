package server

import (
	"AdaptixServer/core/utils/krypt"
	"fmt"
)

func (ts *Teamserver) TsEventTunnelAdd(tunnel *Tunnel) {
	message := ""
	switch tunnel.Type {
	case TUNNEL_SOCKS4:
		message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	case TUNNEL_SOCKS5:
		message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	case TUNNEL_SOCKS5_AUTH:
		message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	case TUNNEL_LPORTFWD:
		message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	default:
		return
	}

	tunnel.TaskId, _ = krypt.GenerateUID(8)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	if tunnel.Data.Client == "" {
		packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
		ts.TsSyncAllClients(packet2)
		ts.events.Put(packet2)
	}
}
