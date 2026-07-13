package server

func (ts *Teamserver) TsFrameHasPending(sessionId int64) bool {
	return ts.FrameManager.HasPending(sessionId)
}

func (ts *Teamserver) TsFramePut(sessionId int64, index uint32, data []byte, totalSize uint32, chunkCount uint16) (bool, uint32, uint32, uint32, []byte) {
	return ts.FrameManager.Put(sessionId, index, data, totalSize, chunkCount)
}

func (ts *Teamserver) TsFrameGetChunk(sessionId int64, reqOffset uint32, maxChunkSize int, encode func([]byte) []byte) (uint32, uint32, []byte, uint32, bool) {
	return ts.FrameManager.GetChunk(sessionId, reqOffset, maxChunkSize, encode)
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
