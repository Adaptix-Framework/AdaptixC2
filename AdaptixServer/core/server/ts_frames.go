package server

import (
	"fmt"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

const ioProgressMinInterval = 250 * time.Millisecond

type ioProgressSent struct {
	last   time.Time
	upF    uint32
	upT    uint32
	dnF    uint32
	dnT    uint32
	active bool
}

func (ts *Teamserver) TsFrameHasPending(sessionId int64) bool {
	return ts.FrameManager.HasPending(sessionId)
}

func (ts *Teamserver) TsAgentIoProgress(agentId int64, upFilled, upTotal, downFilled, downTotal uint32, startedUnix int64, active bool) {
	if ts == nil || agentId == 0 {
		return
	}

	ts.ioProgMu.Lock()
	prev, ok := ts.ioProg[agentId]
	now := time.Now()
	changed := !ok || prev.active != active || prev.upF != upFilled || prev.upT != upTotal || prev.dnF != downFilled || prev.dnT != downTotal
	if !changed {
		ts.ioProgMu.Unlock()
		return
	}
	force := !ok || prev.active != active || !active
	if !force && now.Sub(prev.last) < ioProgressMinInterval {
		ts.ioProgMu.Unlock()
		return
	}
	if active {
		ts.ioProg[agentId] = ioProgressSent{
			last:   now,
			upF:    upFilled,
			upT:    upTotal,
			dnF:    downFilled,
			dnT:    downTotal,
			active: true,
		}
	} else {
		delete(ts.ioProg, agentId)
	}
	ts.ioProgMu.Unlock()

	ts.TsSyncStateWithCategory(
		CreateSpAgentIo(agentId, upFilled, upTotal, downFilled, downTotal, startedUnix, active),
		fmt.Sprintf("agent-io:%d", agentId),
		SyncCategoryAgents,
	)
}

func (ts *Teamserver) TsFramePut(sessionId int64, index uint32, data []byte, totalSize uint32, chunkCount uint16) (bool, uint32, uint32, uint32, []byte) {
	return ts.FrameManager.Put(sessionId, index, data, totalSize, chunkCount)
}

func (ts *Teamserver) TsFramePutDecoded(sessionId int64, index uint32, data []byte, totalSize uint32, chunkCount uint16, decode func(assembled []byte) ([]byte, error)) (bool, uint32, uint32, uint32, []byte) {
	return ts.FrameManager.PutDecoded(sessionId, index, data, totalSize, chunkCount, decode)
}

func (ts *Teamserver) TsFrameGetChunk(sessionId int64, reqOffset uint32, maxChunkSize int, encode func([]byte) []byte) (uint32, uint32, []byte, uint32, bool) {
	return ts.FrameManager.GetChunk(sessionId, reqOffset, maxChunkSize, encode)
}

func (ts *Teamserver) TsFrameGetChunkSticky(sessionId int64, reqOffset uint32, maxChunkSize int, encode func([]byte) []byte) (uint32, uint32, []byte, uint32, bool) {
	return ts.FrameManager.GetChunkSticky(sessionId, reqOffset, maxChunkSize, encode)
}

func (ts *Teamserver) TsFrameTakeStatTasks(sessionId int64) (adaptix.StatTasks, int, bool) {
	return ts.FrameManager.TakeStatTasks(sessionId)
}

func (ts *Teamserver) TsFrameTakeStatRecv(sessionId int64) (int, int, bool) {
	return ts.FrameManager.TakeStatRecv(sessionId)
}

func (ts *Teamserver) TsFrameAckDelivery(sessionId int64, ackOffset uint32, ackNonce uint32) {
	ts.FrameManager.AckDelivery(sessionId, ackOffset, ackNonce)
}

func (ts *Teamserver) TsFramePutStream(sessionId int64, seqNum uint32, data []byte, isLast bool) (bool, []byte) {
	return ts.FrameManager.PutStream(sessionId, seqNum, data, isLast)
}

func (ts *Teamserver) TsFrameResetUpstream(sessionId int64) {
	ts.FrameManager.ResetUpstream(sessionId)
}

func (ts *Teamserver) TsFrameResetDownstream(sessionId int64) {
	ts.FrameManager.ResetDownstream(sessionId)
}
