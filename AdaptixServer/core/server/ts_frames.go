package server

func (ts *Teamserver) TsFrameHasPending(sessionId string) bool {
	return ts.FrameManager.HasPending(sessionId)
}

func (ts *Teamserver) TsFramePut(sessionId string, index uint32, data []byte, totalSize uint32, chunkCount uint16) (bool, uint32, uint32, uint32, []byte) {
	return ts.FrameManager.Put(sessionId, index, data, totalSize, chunkCount)
}

func (ts *Teamserver) TsFrameGetChunk(sessionId string, reqOffset uint32, maxChunkSize int, encode func([]byte) []byte) (uint32, uint32, []byte, uint32, bool) {
	return ts.FrameManager.GetChunk(sessionId, reqOffset, maxChunkSize, encode)
}

func (ts *Teamserver) TsFrameAckDelivery(sessionId string, ackOffset uint32, ackNonce uint32) {
	ts.FrameManager.AckDelivery(sessionId, ackOffset, ackNonce)
}

func (ts *Teamserver) TsFrameResetUpstream(sessionId string) {
	ts.FrameManager.ResetUpstream(sessionId)
}

func (ts *Teamserver) TsFrameResetDownstream(sessionId string) {
	ts.FrameManager.ResetDownstream(sessionId)
}
