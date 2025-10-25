package server

import (
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"fmt"
	"strings"
	"sync"

	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.clients.Contains(username)
}

func (ts *Teamserver) TsClientConnect(username string, version string, socket *websocket.Conn) {
	// Parse version and enable batch sync for v0.11.0+
	supportsBatch := isVersionSupported(version)

	client := &Client{
		username:          username,
		version:           version,
		synced:            false,
		supportsBatchSync: supportsBatch,
		lockSocket:        &sync.Mutex{},
		socket:            socket,
		tmp_store:         safe.NewSlice(),
	}

	ts.clients.Put(username, client)
}

// isVersionSupported checks if client version supports batch sync (>= 0.10.0)
// v0.10 is the current dev branch with batch sync support
// Supports formats: "0.10.0", "v0.10", "Adaptix Framework v0.10", etc.
func isVersionSupported(version string) bool {
	// Empty version means old client without version field (< 0.10.0)
	if version == "" {
		return false
	}

	// Extract version numbers using regex to handle multiple formats
	// Matches: "v0.10.0", "0.10", "Adaptix Framework v0.10", etc.
	var major, minor int

	// Try to find pattern like "v0.10" or "0.10"
	if strings.Contains(version, "v") {
		// Extract after 'v': "Adaptix Framework v0.10" -> "0.10"
		parts := strings.Split(version, "v")
		if len(parts) >= 2 {
			versionNum := strings.TrimSpace(parts[len(parts)-1])
			_, err := fmt.Sscanf(versionNum, "%d.%d", &major, &minor)
			if err != nil {
				return false // Invalid format
			}
		}
	} else {
		// Direct format: "0.11.0" or "0.11"
		_, err := fmt.Sscanf(version, "%d.%d", &major, &minor)
		if err != nil {
			return false // Invalid format
		}
	}

	// Check if version >= 0.10.0 (current dev branch with batch sync)
	if major > 0 {
		return true
	}
	if major == 0 && minor >= 10 {
		return true
	}

	return false
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}
	client := value.(*Client)

	client.synced = false
	client.socket.Close()

	ts.TsEventClient(false, username)

	var tunnels []string
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		if tunnel.Data.Client == username {
			tunnels = append(tunnels, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnels {
		_ = ts.TsTunnelStop(id)
	}
}

func (ts *Teamserver) TsClientSync(username string) {
	value, ok := ts.clients.Get(username)
	if !ok {
		return
	}
	client := value.(*Client)
	socket := client.socket

	if !client.synced {
		ts.TsSyncStored(client)

		for {
			if client.tmp_store.Len() > 0 {
				arr := client.tmp_store.CutArray()
				for _, v := range arr {
					var buffer bytes.Buffer
					_ = json.NewEncoder(&buffer).Encode(v)
					_ = socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
				}
			} else {
				client.synced = true
				break
			}
		}
	}

	ts.TsEventClient(true, username)
}
