package server

import (
	"AdaptixServer/core/eventing"

	"github.com/gorilla/websocket"
)

const SMALL_VERSION = "v1.0"

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.Broker.ClientExists(username)
}

func (ts *Teamserver) TsClientConnect(username string, version string, socket *websocket.Conn) {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataClientConnect{Username: username}
	if !ts.EventManager.Emit(eventing.EventClientConnect, eventing.HookPre, preEvent) {
		return
	}
	// ----------------

	supportsBatchSync := version == SMALL_VERSION

	client := NewClientHandler(username, socket, supportsBatchSync, ts.Broker)
	client.Start()

	ts.Broker.Register(client)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataClientConnect{Username: username}
	ts.EventManager.EmitAsync(eventing.EventClientConnect, postEvent)
	// -----------------
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataClientDisconnect{Username: username}
	ts.EventManager.Emit(eventing.EventClientDisconnect, eventing.HookPre, preEvent)
	// ----------------

	ts.Broker.Unregister(username)

	ts.TsNotifyClient(false, username)

	var tunnelIds []string
	ts.TunnelManager.ForEachTunnel(func(key string, tunnel *Tunnel) bool {
		if tunnel.Data.Client == username {
			tunnelIds = append(tunnelIds, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnelIds {
		_ = ts.TsTunnelStop(id)
	}

	ts.TsProcessHookJobsForDisconnectedClient(username)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataClientDisconnect{Username: username}
	ts.EventManager.EmitAsync(eventing.EventClientDisconnect, postEvent)
	// -----------------
}

func (ts *Teamserver) TsClientSync(username string) {
	client, ok := ts.Broker.GetClient(username)
	if !ok {
		return
	}

	if !client.IsSynced() {
		ts.TsSyncStored(client)

		for {
			buffered := client.GetAndClearBuffer()
			if len(buffered) > 0 {
				for _, v := range buffered {
					if data, ok := v.([]byte); ok {
						client.SendSync(data)
					}
				}
			} else {
				client.SetSynced(true)
				break
			}
		}
	}

	ts.TsNotifyClient(true, username)
}
