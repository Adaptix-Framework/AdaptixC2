package server

import (
	"AdaptixServer/core/eventing"
	"strings"

	"github.com/gorilla/websocket"
)

const SMALL_VERSION = "v1.1"

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.Broker.ClientExists(username)
}

var InitialSyncCategories = []string{
	SyncCategoryExtenders,
	SyncCategoryListeners,
	SyncCategoryAgents,
	SyncCategoryPivots,
}

func (ts *Teamserver) TsClientConnect(username string, version string, socket *websocket.Conn, clientType uint8, consoleTeamMode bool, subscriptions []string) {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataClientConnect{Username: username}
	if !ts.EventManager.Emit(eventing.EventClientConnect, eventing.HookPre, preEvent) {
		return
	}
	// ----------------

	supportsBatchSync := version == SMALL_VERSION

	client := NewClientHandler(username, socket, supportsBatchSync, ts.Broker, clientType, consoleTeamMode)
	client.Start()

	requested := make(map[string]struct{}, len(subscriptions))
	for _, cat := range subscriptions {
		requested[cat] = struct{}{}
	}
	_, hasOnlyActive := requested[SyncCategoryAgentsOnlyActive]
	_, hasAgents := requested[SyncCategoryAgents]
	useOnlyActiveAgents := hasOnlyActive && !hasAgents

	for _, cat := range InitialSyncCategories {
		if cat == SyncCategoryAgents && useOnlyActiveAgents {
			client.Subscribe(SyncCategoryAgentsOnlyActive)
			continue
		}
		client.Subscribe(cat)
	}
	for _, cat := range subscriptions {
		client.Subscribe(cat)
	}

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
		categories := client.GetSubscriptions()
		ts.TsSyncCategories(client, categories)

		for {
			buffered := client.GetAndClearBuffer()
			stateBuffered := client.GetAndClearStateBuffer()

			hasData := len(buffered) > 0 || len(stateBuffered) > 0

			if hasData {
				for _, v := range buffered {
					if data, ok := v.([]byte); ok {
						client.SendSync(data)
					}
				}
				for _, data := range stateBuffered {
					client.SendSync(data)
				}
			} else {
				client.SetSynced(true)
				break
			}
		}
	}

	ts.TsNotifyClient(true, username)
}

func (ts *Teamserver) TsClientSubscribe(username string, categories []string, consoleTeamMode *bool) {
	client, ok := ts.Broker.GetClient(username)
	if !ok {
		return
	}

	if consoleTeamMode != nil {
		client.SetConsoleTeamMode(*consoleTeamMode)
	}

	seen := make(map[string]struct{}, len(categories))
	var newCategories []string
	for _, raw := range categories {
		category := strings.TrimSpace(raw)
		if category == "" {
			continue
		}
		if _, exists := seen[category]; exists {
			continue
		}
		seen[category] = struct{}{}

		if !client.IsSubscribed(category) {
			client.Subscribe(category)
			switch category {
			case SyncCategoryTasksManager,
				SyncCategoryTasksOnlyJobs,
				SyncCategoryChatRealtime,
				SyncCategoryDownloadsRealtime,
				SyncCategoryScreenshotRealtime,
				SyncCategoryCredentialsRealtime,
				SyncCategoryTargetsRealtime:
			default:
				newCategories = append(newCategories, category)
			}
		}
	}

	if len(newCategories) > 0 {
		ts.TsSyncCategories(client, newCategories)
	} else {
		ts.sendSyncPackets(client, nil)
	}
}
