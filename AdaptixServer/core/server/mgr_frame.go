package server

import (
	"AdaptixServer/core/utils/logs"
	"sync"
	"time"

	"math/rand/v2"
)

const (
	frameModeByteOffset = 0
	frameModeChunkIndex = 1
)

type FrameConfig struct {
	MaxUploadSize   uint32
	MaxDownloadSize uint32
	StaleTimeout    time.Duration
	DedupTimeout    time.Duration
	DownTimeout     time.Duration
	MaxAttempts     int
	CleanupInterval time.Duration
}

type FrameManager struct {
	ts *Teamserver

	mu           sync.Mutex
	upSessions   map[string]*upSession   // id → upstream reassembly
	upDoneCache  map[string]*upDoneEntry // id → dedup for completed uploads
	downSessions map[string]*downSession // id → downstream chunked delivery

	active bool

	maxUploadSize   uint32
	maxDownloadSize uint32
	staleTimeout    time.Duration
	dedupTimeout    time.Duration
	downTimeout     time.Duration
	maxAttempts     int
	cleanupInterval time.Duration
}

type upSession struct {
	mode       int // Byte-offset or Chunk-index
	lastUpdate time.Time

	// --- Byte-offset mode ---
	totalSize   uint32
	buf         []byte          // flat reassembly buffer
	filled      uint32          // bytes written so far
	highWater   uint32          // highest byte offset seen
	seenOffsets map[uint32]bool // which byte offsets arrived
	chunkSize   uint32          // size of first chunk

	// --- Chunk-index mode---
	chunkCount uint16            // total expected chunks
	chunks     map[uint16][]byte // received chunks by index
}

type upDoneEntry struct {
	totalSize  uint32
	chunkCount uint16
	doneAt     time.Time
}

type downSession struct {
	buf       []byte
	total     uint32
	taskNonce uint32
	createdAt time.Time
	lastChunk time.Time
	attempts  int
}

//////////

const (
	fmDefaultMaxUpload   = 4 << 20 // 4 MB
	fmDefaultMaxDownload = 4 << 20 // 4 MB

	fmDefaultMaxAttempts  = 10
	fmDefaultStaleTimeout = 5 * time.Minute
	fmDefaultDedupTimeout = 5 * time.Minute
	fmDefaultDownTimeout  = 10 * time.Minute
	fmDefaultCleanup      = 60 * time.Second
)

func NewFrameManager(ts *Teamserver, config *FrameConfig) *FrameManager {
	fm := &FrameManager{
		ts:           ts,
		upSessions:   make(map[string]*upSession),
		upDoneCache:  make(map[string]*upDoneEntry),
		downSessions: make(map[string]*downSession),
		active:       true,

		maxUploadSize:   fmDefaultMaxUpload,
		maxDownloadSize: fmDefaultMaxDownload,
		staleTimeout:    fmDefaultStaleTimeout,
		dedupTimeout:    fmDefaultDedupTimeout,
		downTimeout:     fmDefaultDownTimeout,
		maxAttempts:     fmDefaultMaxAttempts,
		cleanupInterval: fmDefaultCleanup,
	}
	if config != nil {
		if config.MaxUploadSize > 0 {
			fm.maxUploadSize = config.MaxUploadSize
		}
		if config.MaxDownloadSize > 0 {
			fm.maxDownloadSize = config.MaxDownloadSize
		}
		if config.StaleTimeout > 0 {
			fm.staleTimeout = config.StaleTimeout
		}
		if config.DedupTimeout > 0 {
			fm.dedupTimeout = config.DedupTimeout
		}
		if config.DownTimeout > 0 {
			fm.downTimeout = config.DownTimeout
		}
		if config.MaxAttempts > 0 {
			fm.maxAttempts = config.MaxAttempts
		}
		if config.CleanupInterval > 0 {
			fm.cleanupInterval = config.CleanupInterval
		}
	}
	go fm.cleanupLoop()
	return fm
}

func (fm *FrameManager) Stop() {
	fm.mu.Lock()
	fm.active = false
	fm.mu.Unlock()
}

func (fm *FrameManager) Put(sid string, index uint32, data []byte, totalSize uint32, chunkCount uint16) (complete bool, nextExpected uint32, received uint32, sackBitmap uint32, assembled []byte) {
	if sid == "" || (totalSize == 0 && chunkCount == 0) {
		return
	}
	if chunkCount > 0 {
		return fm.putChunkIndex(sid, uint16(index), chunkCount, data)
	}
	return fm.putByteOffset(sid, totalSize, index, data)
}

func (fm *FrameManager) putByteOffset(sid string, totalSize uint32, offset uint32, data []byte) (complete bool, nextExpected uint32, received uint32, sackBitmap uint32, assembled []byte) {
	if totalSize > fm.maxUploadSize {
		return false, 0, 0, 0, nil
	}

	if offset == 0 && totalSize <= uint32(len(data)) {
		result := make([]byte, totalSize)
		copy(result, data[:totalSize])
		return true, totalSize, totalSize, 0, result
	}

	fm.mu.Lock()

	if done, exists := fm.upDoneCache[sid]; exists && done.totalSize == totalSize {
		fm.mu.Unlock()
		return true, totalSize, totalSize, 0, nil
	}

	s, ok := fm.upSessions[sid]
	if !ok || s.mode != frameModeByteOffset || s.totalSize != totalSize || (offset == 0 && s.highWater > 0) {
		s = &upSession{
			mode:        frameModeByteOffset,
			totalSize:   totalSize,
			buf:         make([]byte, totalSize),
			seenOffsets: make(map[uint32]bool),
			lastUpdate:  time.Now(),
		}
		fm.upSessions[sid] = s
	}

	chunkLen := uint32(len(data))
	if s.chunkSize == 0 && chunkLen > 0 {
		s.chunkSize = chunkLen
	}

	if offset >= s.totalSize || s.seenOffsets[offset] {
		ne := fm.computeNextExpectedByteOffset(s)
		fm.mu.Unlock()
		return false, ne, s.filled, 0, nil
	}

	end := offset + chunkLen
	if end > s.totalSize {
		end = s.totalSize
	}
	n := end - offset
	copy(s.buf[offset:end], data[:n])

	s.seenOffsets[offset] = true
	s.filled += n
	s.lastUpdate = time.Now()
	if end > s.highWater {
		s.highWater = end
	}

	nextExp := fm.computeNextExpectedByteOffset(s)

	var completeBuf []byte
	if s.filled >= s.totalSize {
		completeBuf = make([]byte, len(s.buf))
		copy(completeBuf, s.buf)
		fm.upDoneCache[sid] = &upDoneEntry{totalSize: s.totalSize, doneAt: time.Now()}
		delete(fm.upSessions, sid)
		complete = true
		nextExp = s.totalSize
	}

	filled := s.filled
	fm.mu.Unlock()

	return complete, nextExp, filled, 0, completeBuf
}

func (fm *FrameManager) putChunkIndex(sid string, chunkIdx uint16, chunkCount uint16, data []byte) (complete bool, nextExpected uint32, received uint32, sackBitmap uint32, assembled []byte) {
	if chunkCount == 0 {
		return
	}

	fm.mu.Lock()

	if done, exists := fm.upDoneCache[sid]; exists && done.chunkCount == chunkCount {
		fm.mu.Unlock()
		return true, uint32(chunkCount), uint32(chunkCount), 0, nil
	}

	s, ok := fm.upSessions[sid]
	if !ok || s.mode != frameModeChunkIndex || s.chunkCount != chunkCount {
		s = &upSession{
			mode:       frameModeChunkIndex,
			chunkCount: chunkCount,
			chunks:     make(map[uint16][]byte),
			lastUpdate: time.Now(),
		}
		fm.upSessions[sid] = s
	}

	if _, exists := s.chunks[chunkIdx]; !exists {
		chunk := make([]byte, len(data))
		copy(chunk, data)
		s.chunks[chunkIdx] = chunk
	}
	s.lastUpdate = time.Now()

	receivedCount := uint16(len(s.chunks))
	complete = receivedCount >= chunkCount

	var nextExp uint16
	if !complete {
		for i := uint16(0); i < chunkCount; i++ {
			if _, ok := s.chunks[i]; !ok {
				nextExp = i
				break
			}
		}
		for bit := 0; bit < 32; bit++ {
			ci := nextExp + 1 + uint16(bit)
			if ci >= chunkCount {
				break
			}
			if _, ok := s.chunks[ci]; ok {
				sackBitmap |= 1 << uint(bit)
			}
		}
	} else {
		nextExp = chunkCount
	}

	if complete {
		for i := uint16(0); i < chunkCount; i++ {
			assembled = append(assembled, s.chunks[i]...)
		}
		fm.upDoneCache[sid] = &upDoneEntry{chunkCount: chunkCount, doneAt: time.Now()}
		delete(fm.upSessions, sid)
	}

	fm.mu.Unlock()

	return complete, uint32(nextExp), uint32(receivedCount), sackBitmap, assembled
}

func (fm *FrameManager) GetChunk(sid string, reqOffset uint32, maxChunkSize int, encode func([]byte) []byte) (total uint32, chunkOffset uint32, data []byte, taskNonce uint32, isEmpty bool) {
	if sid == "" || maxChunkSize <= 0 {
		return 0, 0, nil, 0, true
	}

	fm.mu.Lock()

	ds := fm.downSessions[sid]

	if ds != nil && reqOffset >= ds.total {
		delete(fm.downSessions, sid)
		ds = nil
	}

	if ds == nil {
		fm.mu.Unlock()

		packed, err := fm.ts.TsAgentGetHostedAll(sid, int(fm.maxDownloadSize))
		if err != nil || len(packed) == 0 {
			return 0, 0, nil, 0, true
		}

		payload := packed
		if encode != nil {
			payload = encode(packed)
		}

		nonce := uint32(time.Now().UnixNano()&0xFFFFFFFF) ^ rand.Uint32()

		ds = &downSession{
			buf:       payload,
			total:     uint32(len(payload)),
			taskNonce: nonce,
			createdAt: time.Now(),
			lastChunk: time.Now(),
		}

		fm.mu.Lock()
		if existing := fm.downSessions[sid]; existing != nil {
			ds = existing
		} else {
			fm.downSessions[sid] = ds
		}
	}

	ds.lastChunk = time.Now()
	ds.attempts++

	if reqOffset >= ds.total {
		fm.mu.Unlock()
		return ds.total, reqOffset, nil, ds.taskNonce, true
	}

	remaining := ds.total - reqOffset
	chunkLen := remaining
	if chunkLen > uint32(maxChunkSize) {
		chunkLen = uint32(maxChunkSize)
	}

	chunk := make([]byte, chunkLen)
	copy(chunk, ds.buf[reqOffset:reqOffset+chunkLen])
	t := ds.total
	nonce := ds.taskNonce

	fm.mu.Unlock()

	return t, reqOffset, chunk, nonce, false
}

func (fm *FrameManager) HasPending(sid string) bool {
	fm.mu.Lock()
	ds := fm.downSessions[sid]
	fm.mu.Unlock()

	if ds != nil {
		return true
	}

	agent, err := fm.ts.getAgent(sid)
	if err != nil {
		return false
	}
	return agent.HostedQueue.Len() > 0 || (agent.PivotChilds.Len() > 0 && fm.ts.TsTasksPivotExists(sid, true))
}

func (fm *FrameManager) AckDelivery(sid string, ackOffset uint32, ackNonce uint32) {
	fm.mu.Lock()
	defer fm.mu.Unlock()

	ds := fm.downSessions[sid]
	if ds == nil {
		return
	}
	if ackNonce != 0 && ds.taskNonce != ackNonce {
		return
	}
	if ackOffset >= ds.total {
		delete(fm.downSessions, sid)
	}
}

func (fm *FrameManager) ResetUpstream(sid string) {
	fm.mu.Lock()
	defer fm.mu.Unlock()
	delete(fm.upSessions, sid)
}

func (fm *FrameManager) ResetDownstream(sid string) {
	fm.mu.Lock()
	defer fm.mu.Unlock()
	delete(fm.downSessions, sid)
}

func (fm *FrameManager) computeNextExpectedByteOffset(s *upSession) uint32 {
	if s.chunkSize == 0 {
		return s.highWater
	}
	for off := uint32(0); off < s.totalSize; off += s.chunkSize {
		if !s.seenOffsets[off] {
			return off
		}
	}
	return s.totalSize
}

func (fm *FrameManager) cleanupLoop() {
	ticker := time.NewTicker(fm.cleanupInterval)
	defer ticker.Stop()

	for {
		<-ticker.C
		fm.mu.Lock()
		if !fm.active {
			fm.mu.Unlock()
			return
		}
		fm.cleanupStale()
		fm.mu.Unlock()
	}
}

func (fm *FrameManager) cleanupStale() {
	now := time.Now()

	for sid, s := range fm.upSessions {
		if now.Sub(s.lastUpdate) > fm.staleTimeout {
			delete(fm.upSessions, sid)
		}
	}

	for sid, entry := range fm.upDoneCache {
		if now.Sub(entry.doneAt) > fm.dedupTimeout {
			delete(fm.upDoneCache, sid)
		}
	}

	for sid, ds := range fm.downSessions {
		if now.Sub(ds.lastChunk) > fm.downTimeout || ds.attempts > fm.maxAttempts {
			delete(fm.downSessions, sid)
		}
	}
}
