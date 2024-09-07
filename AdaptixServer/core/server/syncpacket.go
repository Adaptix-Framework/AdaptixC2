package server

import "time"

const (
	storeLog = "log"

	typeSync       = "sync"
	typeSyncStart  = "start"
	typeSyncFinish = "finish"

	typeClient           = "client"
	typeClientConnect    = "connect"
	typeClientDisconnect = "disconnect"
)

func CreateSpSyncStart(count int) SyncPackerStart {
	return SyncPackerStart{
		SpType:    typeSync,
		SpSubType: typeSyncStart,

		Count: count,
	}
}

func CreateSpSyncFinish() SyncPackerFinish {
	return SyncPackerFinish{
		SpType:    typeSync,
		SpSubType: typeSyncFinish,
	}
}

func CreateSpClientConnect(username string) SyncPackerClientConnect {
	return SyncPackerClientConnect{
		store:        storeLog,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       typeClient,
		SpSubType:    typeClientConnect,

		Username: username,
	}
}

func CreateSpClientDisconnect(username string) SyncPackerClientDisconnect {
	return SyncPackerClientDisconnect{
		store:        storeLog,
		SpCreateTime: time.Now().UTC().Unix(),
		SpType:       typeClient,
		SpSubType:    typeClientDisconnect,

		Username: username,
	}
}
