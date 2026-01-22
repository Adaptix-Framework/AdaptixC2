package main

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/vmihailenco/msgpack/v5"
)

type Listener struct {
	transport *TransportTCP
}

type TransportTCP struct {
	AgentConnects Map
	JobConnects   Map
	Listener      net.Listener
	Config        TransportConfig
	Name          string
	Active        bool
}

type TransportConfig struct {
	HostBind           string `json:"host_bind"`
	PortBind           int    `json:"port_bind"`
	Callback_addresses string `json:"callback_addresses"`
	EncryptKey         string `json:"encrypt_key"`

	Ssl        bool   `json:"ssl"`
	CaCert     []byte `json:"ca_cert"`
	ServerCert []byte `json:"server_cert"`
	ServerKey  []byte `json:"server_key"`
	ClientCert []byte `json:"client_cert"`
	ClientKey  []byte `json:"client_key"`

	TcpBanner   string `json:"tcp_banner"`
	ErrorAnswer string `json:"error_answer"`
	Timeout     int    `json:"timeout"`

	Protocol string `json:"protocol"`
}

type Connection struct {
	conn         net.Conn
	ctx          context.Context
	handleCancel context.CancelFunc
}

const (
	INIT_PACK     = 1
	EXFIL_PACK    = 2
	JOB_PACK      = 3
	TUNNEL_PACK   = 4
	TERMINAL_PACK = 5
)

type StartMsg struct {
	Type int    `msgpack:"id"`
	Data []byte `msgpack:"data"`
}

type InitPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Data []byte `msgpack:"data"`
}

type ExfilPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Task string `msgpack:"task"`
}

type JobPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Task string `msgpack:"task"`
}

type TunnelPack struct {
	Id        uint   `msgpack:"id"`
	Type      uint   `msgpack:"type"`
	ChannelId int    `msgpack:"channel_id"`
	Key       []byte `msgpack:"key"`
	Iv        []byte `msgpack:"iv"`
	Alive     bool   `msgpack:"alive"`
	Reason    byte   `msgpack:"reason"`
}

type TermPack struct {
	Id     uint   `msgpack:"id"`
	TermId int    `msgpack:"term_id"`
	Key    []byte `msgpack:"key"`
	Iv     []byte `msgpack:"iv"`
	Alive  bool   `msgpack:"alive"`
	Status string `msgpack:"status"`
}

func validConfig(config string) error {
	var conf TransportConfig
	err := json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("HostBind is required")
	}

	if conf.PortBind < 1 || conf.PortBind > 65535 {
		return errors.New("PortBind must be in the range 1-65535")
	}

	if conf.Callback_addresses == "" {
		return errors.New("callback_servers is required")
	}
	lines := strings.Split(strings.TrimSpace(conf.Callback_addresses), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		host, portStr, err := net.SplitHostPort(line)
		if err != nil {
			return fmt.Errorf("Invalid address (cannot split host:port): %s\n", line)
		}

		port, err := strconv.Atoi(portStr)
		if err != nil || port < 1 || port > 65535 {
			return fmt.Errorf("Invalid port: %s\n", line)
		}

		ip := net.ParseIP(host)
		if ip == nil {
			if len(host) == 0 || len(host) > 253 {
				return fmt.Errorf("Invalid host: %s\n", line)
			}
			parts := strings.Split(host, ".")
			for _, part := range parts {
				if len(part) == 0 || len(part) > 63 {
					return fmt.Errorf("Invalid host: %s\n", line)
				}
			}
		}
	}

	if conf.Timeout < 1 {
		return errors.New("Timeout must be greater than 0")
	}

	match, _ := regexp.MatchString("^[0-9a-f]{32}$", conf.EncryptKey)
	if len(conf.EncryptKey) != 32 || !match {
		return errors.New("encrypt_key must be 32 hex characters")
	}

	return nil
}

func (t *TransportTCP) Start(ts Teamserver) error {
	var err error = nil
	address := fmt.Sprintf("%s:%d", t.Config.HostBind, t.Config.PortBind)

	if t.Config.Ssl {
		fmt.Printf("  Started mTLS listener '%s': %s\n", t.Name, address)
		cert, err := tls.X509KeyPair(t.Config.ServerCert, t.Config.ServerKey)
		if err != nil {
			return err
		}

		caCertPool := x509.NewCertPool()
		caCertPool.AppendCertsFromPEM(t.Config.CaCert)
		config := &tls.Config{
			Certificates: []tls.Certificate{cert},
			ClientAuth:   tls.RequireAndVerifyClientCert, // Require client verification
			ClientCAs:    caCertPool,
		}

		t.Listener, err = tls.Listen("tcp", address, config)
		if err != nil {
			return err
		}

	} else {
		fmt.Printf("  Started TCP listener '%s': %s\n", t.Name, address)
		t.Listener, err = net.Listen("tcp", address)
		if err != nil {
			return err
		}
	}

	go func() {
		for {
			conn, err := t.Listener.Accept()
			if err != nil {
				return
			}
			go t.handleConnection(conn, ts)
		}
	}()
	t.Active = true

	time.Sleep(500 * time.Millisecond)
	return err
}

func (t *TransportTCP) handleConnection(conn net.Conn, ts Teamserver) {
	var (
		sendData []byte
		recvData []byte
		encKey   []byte
		err      error
		initMsg  StartMsg
	)

	if len(t.Config.TcpBanner) > 0 {
		_, _ = conn.Write([]byte(t.Config.TcpBanner))
	}

	connection := Connection{
		conn: conn,
	}
	connection.ctx, connection.handleCancel = context.WithCancel(context.Background())

	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	recvData, err = recvMsg(conn)
	_ = conn.SetReadDeadline(time.Time{})
	if err != nil {
		goto ERR
	}

	encKey, err = hex.DecodeString(t.Config.EncryptKey)
	if err != nil {
		goto ERR
	}
	recvData, err = DecryptData(recvData, encKey)
	if err != nil {
		goto ERR
	}

	err = msgpack.Unmarshal(recvData, &initMsg)
	if err != nil {
		goto ERR
	}

	switch initMsg.Type {

	case INIT_PACK:

		var initPack InitPack
		err := msgpack.Unmarshal(initMsg.Data, &initPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", initPack.Id)
		agentType := fmt.Sprintf("%08x", initPack.Type)
		ExternalIP := strings.Split(conn.RemoteAddr().String(), ":")[0]

		if !Ts.TsAgentIsExists(agentId) {
			_, err = Ts.TsAgentCreate(agentType, agentId, initPack.Data, t.Name, ExternalIP, false)
			if err != nil {
				goto ERR
			}
		} else {
			emptyMark := ""
			_ = Ts.TsAgentUpdateDataPartial(agentId, struct {
				Mark *string `json:"mark"`
			}{Mark: &emptyMark})
		}

		t.AgentConnects.Put(agentId, connection)

		for {
			sendData, err = Ts.TsAgentGetHostedTasks(agentId, 0x1900000)
			if err != nil {
				break
			}

			if sendData != nil && len(sendData) > 0 {
				err = sendMsg(conn, sendData)
				if err != nil {
					break
				}

				recvData, err = recvMsg(conn)
				if err != nil {
					break
				}

				_ = Ts.TsAgentSetTick(agentId, t.Name)

				_ = Ts.TsAgentProcessData(agentId, recvData)
			} else {
				if !isClientConnected(conn, t.Config.Ssl) {
					break
				}
			}
		}

		disconnectMark := "Disconnect"
		_ = ts.TsAgentUpdateDataPartial(agentId, struct {
			Mark *string `json:"mark"`
		}{Mark: &disconnectMark})
		t.AgentConnects.Delete(agentId)
		_ = conn.Close()

	case EXFIL_PACK:

		var exfilPack ExfilPack
		err := msgpack.Unmarshal(initMsg.Data, &exfilPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", exfilPack.Id)

		if !Ts.TsTaskRunningExists(agentId, exfilPack.Task) {
			goto ERR
		}

		jcId := agentId + "-" + exfilPack.Task

		t.JobConnects.Put(jcId, connection)

		for {
			recvData, err = recvMsg(conn)
			if err != nil {
				break
			}
			_ = Ts.TsAgentProcessData(agentId, recvData)
		}

		t.JobConnects.Delete(jcId)
		_ = conn.Close()

	case JOB_PACK:

		var jobPack JobPack
		err := msgpack.Unmarshal(initMsg.Data, &jobPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", jobPack.Id)

		if !Ts.TsTaskRunningExists(agentId, jobPack.Task) {
			goto ERR
		}

		jcId := agentId + "-" + jobPack.Task

		t.JobConnects.Put(jcId, connection)

		for {
			recvData, err = recvMsg(conn)
			if err != nil {
				break
			}
			_ = Ts.TsAgentProcessData(agentId, recvData)
		}

		t.JobConnects.Delete(jcId)
		_ = conn.Close()

	case TUNNEL_PACK:

		var tunPack TunnelPack
		err := msgpack.Unmarshal(initMsg.Data, &tunPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", tunPack.Id)

		if !Ts.TsTunnelChannelExists(tunPack.ChannelId) {
			goto ERR
		}

		if !tunPack.Alive {
			if tunPack.Reason < 1 || tunPack.Reason > 8 {
				tunPack.Reason = 5
			}
			ts.TsTunnelConnectionHalt(tunPack.ChannelId, tunPack.Reason)
			_ = conn.Close()
			return
		}

		ts.TsTunnelConnectionResume(agentId, tunPack.ChannelId, true)

		pr, pw, err := Ts.TsTunnelGetPipe(agentId, tunPack.ChannelId)
		if err != nil {
			goto ERR
		}

		blockEnc, _ := aes.NewCipher(tunPack.Key)
		encStream := cipher.NewCTR(blockEnc, tunPack.Iv)
		encWriter := &cipher.StreamWriter{S: encStream, W: conn}

		blockDec, _ := aes.NewCipher(tunPack.Key)
		decStream := cipher.NewCTR(blockDec, tunPack.Iv)
		decWriter := &cipher.StreamWriter{S: decStream, W: pw}

		var closeOnce sync.Once
		closeAll := func() {
			closeOnce.Do(func() {
				_ = conn.Close()
				_ = pr.Close()
			})
		}

		var wg sync.WaitGroup

		wg.Add(1)
		go func() {
			defer wg.Done()
			io.Copy(encWriter, pr)
			closeAll()
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()
			io.Copy(decWriter, conn)
			closeAll()
		}()

		wg.Wait()

		ts.TsTunnelConnectionClose(tunPack.ChannelId, false)

	case TERMINAL_PACK:

		var termPack TermPack
		err := msgpack.Unmarshal(initMsg.Data, &termPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", termPack.Id)
		terminalId := fmt.Sprintf("%08x", termPack.TermId)

		if !Ts.TsTerminalConnExists(terminalId) {
			goto ERR
		}

		if !termPack.Alive {
			_ = ts.TsAgentTerminalCloseChannel(terminalId, termPack.Status)
			_ = conn.Close()
			return
		}

		ts.TsTerminalConnResume(agentId, terminalId, true)

		pr, pw, err := Ts.TsTerminalGetPipe(agentId, terminalId)
		if err != nil {
			goto ERR
		}

		blockEnc, _ := aes.NewCipher(termPack.Key)
		encStream := cipher.NewCTR(blockEnc, termPack.Iv)
		encWriter := &cipher.StreamWriter{S: encStream, W: conn}

		blockDec, _ := aes.NewCipher(termPack.Key)
		decStream := cipher.NewCTR(blockDec, termPack.Iv)
		decWriter := &cipher.StreamWriter{S: decStream, W: pw}

		var closeOnce sync.Once
		closeAll := func() {
			closeOnce.Do(func() {
				_ = conn.Close()
				_ = pr.Close()
			})
		}

		var wg sync.WaitGroup

		wg.Add(1)
		go func() {
			defer wg.Done()
			io.Copy(encWriter, pr)
			closeAll()
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()
			io.Copy(decWriter, conn)
			closeAll()
		}()

		wg.Wait()

		_ = ts.TsAgentTerminalCloseChannel(terminalId, "killed")
	}

	return

ERR:
	_ = sendMsg(conn, []byte(t.Config.ErrorAnswer))
	_ = conn.Close()
}

func (t *TransportTCP) Stop() error {
	var (
		err          error = nil
		listenerPath       = ListenerDataDir + "/" + t.Name
	)

	if t.Listener != nil {
		_ = t.Listener.Close()
	}

	t.AgentConnects.ForEach(func(key string, valueConn interface{}) bool {
		connection, _ := valueConn.(Connection)
		if connection.conn != nil {
			connection.handleCancel()
			_ = connection.conn.Close()
		}
		return true
	})

	_, err = os.Stat(listenerPath)
	if err == nil {
		err = os.RemoveAll(listenerPath)
		if err != nil {
			return fmt.Errorf("failed to remove %s folder: %s", listenerPath, err.Error())
		}
	}

	return nil
}

func (t *TransportTCP) bannerError(conn net.Conn) {
	_, _ = conn.Write([]byte(t.Config.ErrorAnswer))
	_ = conn.Close()
}

func connRead(conn net.Conn, size int) ([]byte, error) {
	if size <= 0 {
		return nil, fmt.Errorf("incorrected size: %d", size)
	}

	message := make([]byte, 0, size)
	tmpBuff := make([]byte, 1024)
	readSize := 0

	for readSize < size {
		toRead := size - readSize
		if toRead < len(tmpBuff) {
			tmpBuff = tmpBuff[:toRead]
		}

		n, err := conn.Read(tmpBuff)
		if err != nil {
			return nil, err
		}

		message = append(message, tmpBuff[:n]...)
		readSize += n
	}
	return message, nil
}

func recvMsg(conn net.Conn) ([]byte, error) {
	bufLen, err := connRead(conn, 4)
	if err != nil {
		return nil, err
	}
	msgLen := binary.BigEndian.Uint32(bufLen)

	return connRead(conn, int(msgLen))
}

func sendMsg(conn net.Conn, data []byte) error {
	if conn == nil {
		return errors.New("conn is nil")
	}

	msgLen := make([]byte, 4)
	binary.BigEndian.PutUint32(msgLen, uint32(len(data)))
	message := append(msgLen, data...)
	_, err := conn.Write(message)
	return err
}

func isClientConnected(conn net.Conn, ssl bool) bool {
	var err error = nil
	if ssl {
		tcpConn, ok := conn.(*tls.Conn)
		if !ok {
			return false
		}
		_ = tcpConn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
		buf := make([]byte, 1)
		_, err = conn.Read(buf)
		_ = tcpConn.SetReadDeadline(time.Time{})

	} else {
		tcpConn, ok := conn.(*net.TCPConn)
		if !ok {
			return false
		}
		_ = tcpConn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
		buf := make([]byte, 1)
		_, err = conn.Read(buf)
		_ = tcpConn.SetReadDeadline(time.Time{})
	}

	if err != nil {
		if err.Error() == "EOF" || errors.Is(err, net.ErrClosed) {
			return false
		}
	}

	return true
}

func EncryptData(data []byte, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonce := make([]byte, gcm.NonceSize())
	_, err = io.ReadFull(rand.Reader, nonce)
	if err != nil {
		return nil, err
	}
	ciphertext := gcm.Seal(nonce, nonce, data, nil)

	return ciphertext, nil
}

func DecryptData(data []byte, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonceSize := gcm.NonceSize()
	if len(data) < nonceSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	nonce, ciphertext := data[:nonceSize], data[nonceSize:]

	plaintext, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return nil, err
	}
	return plaintext, nil
}
