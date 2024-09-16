package server

import "time"

const (
	STORE_LOG  = "log"
	STORE_INIT = "init"

	TYPE_SYNC        = 10
	TYPE_SYNC_START  = 11
	TYPE_SYNC_FINISH = 12

	TYPE_CLIENT            = 20
	TYPE_CLIENT_CONNECT    = 21
	TYPE_CLIENT_DISCONNECT = 22

	TYPE_LISTENER     = 30
	TYPE_LISTENER_NEW = 31
)

/// SYNC

func CreateSpSyncStart(count int) SyncPackerStart {
	return SyncPackerStart{
		SpType:    TYPE_SYNC,
		SpSubType: TYPE_SYNC_START,

		Count: count,
	}
}

func CreateSpSyncFinish() SyncPackerFinish {
	return SyncPackerFinish{
		SpType:    TYPE_SYNC,
		SpSubType: TYPE_SYNC_FINISH,
	}
}

/// CLIENT

func CreateSpClientConnect(username string) SyncPackerClientConnect {
	return SyncPackerClientConnect{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_CLIENT,
		SpSubType:    TYPE_CLIENT_CONNECT,

		Username: username,
	}
}

func CreateSpClientDisconnect(username string) SyncPackerClientDisconnect {
	return SyncPackerClientDisconnect{
		store:        STORE_LOG,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       TYPE_CLIENT,
		SpSubType:    TYPE_CLIENT_DISCONNECT,

		Username: username,
	}
}

/// LISTENER

func CreateSpListenerNew(fn string, ui string) SyncPackerListenerNew {
	return SyncPackerListenerNew{
		store:     STORE_INIT,
		SpType:    TYPE_LISTENER,
		SpSubType: TYPE_LISTENER_NEW,

		ListenerFN: fn,
		ListenerUI: ui,
	}
}
