package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net"
	"strconv"
	"time"
)

func (ts *Teamserver) TsTunnelAdd(tunnel *Tunnel) {

	message := ""
	if tunnel.Data.Type == "SOCKS5 proxy" {
		message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS4 proxy" {
		message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS5 Auth proxy" {
		message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "Local port forward" {
		message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	}

	tunnel.TaskId, _ = krypt.GenerateUID(8)

	ts.tunnels.Put(tunnel.Data.TunnelId, tunnel)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)
}

func (ts *Teamserver) TsReverseAdd(tunnel *Tunnel) {
	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	message := fmt.Sprintf("Reverse port forward to '%s:%s'", tunnel.Data.Fhost, tunnel.Data.Fport)
	packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)
}

func (ts *Teamserver) TsTunnelStop(TunnelId string) error {
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.listener != nil {
		_ = tunnel.listener.Close()
	}

	tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
		tunnelConnection, _ := valueConn.(*TunnelConnection)
		if tunnelConnection.conn != nil {
			tunnelConnection.handleCancel()
			_ = tunnelConnection.conn.Close()
		}
		return true
	})

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClients(packet)

	taskData := adaptix.TaskData{
		TaskId:     tunnel.TaskId,
		Completed:  true,
		FinishDate: time.Now().Unix(),
	}
	var taskBuffer bytes.Buffer
	_ = json.NewEncoder(&taskBuffer).Encode(taskData)
	ts.TsTaskUpdate(tunnel.Data.AgentId, taskBuffer.Bytes())

	message := ""
	if tunnel.Data.Type == "SOCKS5 proxy" {
		message = fmt.Sprintf("SOCKS5 server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS4 proxy" {
		message = fmt.Sprintf("SOCKS4 server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS5 Auth proxy" {
		message = fmt.Sprintf("SOCKS5 (with Auth) server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "Local port forward" {
		message = fmt.Sprintf("Local port forward on ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "Remote port forward" {
		message = fmt.Sprintf("Remote port forward to '%s:%s' stopped", tunnel.Data.Fhost, tunnel.Data.Fport)
	}

	packet2 := CreateSpEvent(EVENT_TUNNEL_STOP, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

	return nil
}

func (ts *Teamserver) TsTunnelSetInfo(TunnelId string, Info string) error {
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.Data.Info = Info

	packet := CreateSpTunnelEdit(tunnel.Data)
	ts.TsSyncAllClients(packet)

	return nil
}

/// Socks5

func (ts *Teamserver) TsTunnelCreateSocks4(AgentId string, Address string, Port int, FuncMsgConnectTCP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error) {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return "", err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnectTCP: FuncMsgConnectTCP,
		handlerWriteTCP:   FuncMsgWriteTCP,
		handlerClose:      FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				return
			}
			go handleRequestSocks4(agent, socksTunnel, conn)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return "", err
	}

	id := krypt.CRC32([]byte(agent.Data.Id + "socks" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	socksTunnel.Data = adaptix.TunnelData{
		TunnelId:  tunnelId,
		AgentId:   agent.Data.Id,
		Computer:  agent.Data.Computer,
		Username:  agent.Data.Username,
		Process:   agent.Data.Process,
		Type:      "SOCKS4 proxy",
		Info:      "",
		Interface: Address,
		Port:      port,
		Client:    "",
		Fport:     "",
		Fhost:     "",
	}

	ts.TsTunnelAdd(socksTunnel)

	return socksTunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks5(AgentId string, Address string, Port int, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error) {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return "", err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnectTCP: FuncMsgConnectTCP,
		handlerConnectUDP: FuncMsgConnectUDP,
		handlerWriteTCP:   FuncMsgWriteTCP,
		handlerWriteUDP:   FuncMsgWriteUDP,
		handlerClose:      FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				return
			}
			go handleRequestSocks5(agent, socksTunnel, conn)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return "", err
	}

	id := krypt.CRC32([]byte(agent.Data.Id + "socks" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	socksTunnel.Data = adaptix.TunnelData{
		TunnelId:  tunnelId,
		AgentId:   agent.Data.Id,
		Computer:  agent.Data.Computer,
		Username:  agent.Data.Username,
		Process:   agent.Data.Process,
		Type:      "SOCKS5 proxy",
		Info:      "",
		Interface: Address,
		Port:      port,
		Client:    "",
		Fport:     "",
		Fhost:     "",
	}

	ts.TsTunnelAdd(socksTunnel)

	return socksTunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error) {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return "", err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnectTCP: FuncMsgConnectTCP,
		handlerConnectUDP: FuncMsgConnectUDP,
		handlerWriteTCP:   FuncMsgWriteTCP,
		handlerWriteUDP:   FuncMsgWriteUDP,
		handlerClose:      FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				return
			}
			go handleRequestSocks5Auth(agent, socksTunnel, conn, Username, Password)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return "", err
	}

	id := krypt.CRC32([]byte(agent.Data.Id + "socks" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	socksTunnel.Data = adaptix.TunnelData{
		TunnelId:  tunnelId,
		AgentId:   agent.Data.Id,
		Computer:  agent.Data.Computer,
		Username:  agent.Data.Username,
		Process:   agent.Data.Process,
		Type:      "SOCKS5 Auth proxy",
		Info:      fmt.Sprintf("%s : %s", Username, Password),
		Interface: Address,
		Port:      port,
		Client:    "",
		Fport:     "",
		Fhost:     "",
	}

	ts.TsTunnelAdd(socksTunnel)

	return socksTunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

/// Port Forward

func (ts *Teamserver) TsTunnelCreateLocalPortFwd(AgentId string, Address string, Port int, FwdAddress string, FwdPort int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error) {
	var (
		agent     *Agent
		fwdTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return "", err
	}

	fwdTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnectTCP: FuncMsgConnect,
		handlerWriteTCP:   FuncMsgWrite,
		handlerClose:      FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = fwdTunnel.listener.Accept()
			if err != nil {
				return
			}
			go handleLocalPortFwd(agent, fwdTunnel, conn, FwdAddress, FwdPort)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return "", err
	}

	id := krypt.CRC32([]byte(agent.Data.Id + "lportfwd" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	fwdTunnel.Data = adaptix.TunnelData{
		TunnelId:  tunnelId,
		AgentId:   agent.Data.Id,
		Computer:  agent.Data.Computer,
		Username:  agent.Data.Username,
		Process:   agent.Data.Process,
		Type:      "Local port forward",
		Info:      "",
		Interface: Address,
		Port:      port,
		Client:    "",
		Fport:     strconv.Itoa(FwdPort),
		Fhost:     FwdAddress,
	}

	ts.TsTunnelAdd(fwdTunnel)

	return fwdTunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelStopLocalPortFwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "lportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

func (ts *Teamserver) TsTunnelCreateRemotePortFwd(AgentId string, Port int, FwdAddress string, FwdPort int, FuncMsgReverse func(tunnelId int, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error) {
	var (
		agent     *Agent
		fwdTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	fport := strconv.Itoa(FwdPort)

	fwdTunnel = &Tunnel{
		connections: safe.NewMap(),

		handlerWriteTCP: FuncMsgWrite,
		handlerClose:    FuncMsgClose,
	}
	fwdTunnel.TaskId, _ = krypt.GenerateUID(8)

	id := krypt.CRC32([]byte(agent.Data.Id + "rportfwd" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	fwdTunnel.Data = adaptix.TunnelData{
		TunnelId:  tunnelId,
		AgentId:   agent.Data.Id,
		Computer:  agent.Data.Computer,
		Username:  agent.Data.Username,
		Process:   agent.Data.Process,
		Type:      "Reverse port forward",
		Info:      "",
		Interface: "",
		Port:      port,
		Client:    "",
		Fport:     fport,
		Fhost:     FwdAddress,
	}

	rawTaskData := FuncMsgReverse(int(id), Port)
	sendTunnelTaskData(agent, rawTaskData)

	ts.tunnels.Put(fwdTunnel.Data.TunnelId, fwdTunnel)

	return fwdTunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelStateRemotePortFwd(tunnelId int, result bool) (string, string, error) {
	var (
		tunnel     *Tunnel
		value      interface{}
		ok         bool
		taskBuffer bytes.Buffer
	)

	tunId := fmt.Sprintf("%08x", tunnelId)

	if result == true {
		value, ok = ts.tunnels.Get(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

			message := fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)

			packet := CreateSpTunnelCreate(tunnel.Data)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
			ts.TsSyncAllClients(packet2)
			ts.events.Put(packet2)

			return tunnel.TaskId, message, nil
		}
	} else {
		value, ok = ts.tunnels.GetDelete(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

			taskData := adaptix.TaskData{
				TaskId:      tunnel.TaskId,
				MessageType: CONSOLE_OUT_ERROR,
				Message:     "This port is already in use",
				FinishDate:  time.Now().Unix(),
				Completed:   true,
			}
			_ = json.NewEncoder(&taskBuffer).Encode(taskData)
			ts.TsTaskUpdate(tunnel.Data.AgentId, taskBuffer.Bytes())
			return tunnel.TaskId, "", errors.New("this port is already in use")
		}
	}
	return "", "", errors.New("tunnel not found")
}

func (ts *Teamserver) TsTunnelStopRemotePortFwd(AgentId string, Port int) {
	var (
		tunnel *Tunnel
		agent  *Agent
		value  interface{}
		ok     bool
	)

	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "rportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	value, ok = ts.tunnels.Get(TunnelId)
	if !ok {
		return
	}
	tunnel, _ = value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ = value.(*Agent)

	rawTaskData := tunnel.handlerClose(int(id))
	sendTunnelTaskData(agent, rawTaskData)

	_ = ts.TsTunnelStop(TunnelId)
}

/// Connection

func (ts *Teamserver) TsTunnelConnectionData(channelId int, data []byte) {
	var (
		valueConn        interface{}
		tunnelConnection *TunnelConnection
		ok               bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ := valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		go tunnelDataToSocket(tunnelConnection, data)
	}
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int) {
	var (
		valueConn        interface{}
		tunnel           *Tunnel
		tunnelConnection *TunnelConnection
		ok               bool
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		go socketToTunnelData(agent, tunnel, tunnelConnection)
	}
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int) {
	var (
		valueConn        interface{}
		tunnel           *Tunnel
		tunnelConnection *TunnelConnection
		ok               bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		tunnelConnection.handleCancel()
		_ = tunnelConnection.conn.Close()
		tunnel.connections.Delete(strconv.Itoa(channelId))
	}
}

func (ts *Teamserver) TsTunnelConnectionAccept(tunnelId int, channelId int) {
	var (
		value  interface{}
		tunnel *Tunnel
		agent  *Agent
		ok     bool
	)

	tunId := fmt.Sprintf("%08x", tunnelId)
	value, ok = ts.tunnels.Get(tunId)
	if !ok {
		return
	}
	tunnel, _ = value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ = value.(*Agent)

	handlerReverseAccept(agent, tunnel, channelId)
}

/// handlers

func handleRequestSocks4(agent *Agent, tunnel *Tunnel, socksConn net.Conn) {

	targetAddress, targetPort, err := proxy.CheckSocks4(socksConn)
	if err != nil {
		fmt.Println("Socks4 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	rawTaskData := tunnel.handlerConnectTCP(channelId, targetAddress, targetPort)
	sendTunnelTaskData(agent, rawTaskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
		protocol:  "TCP",
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handleRequestSocks5(agent *Agent, tunnel *Tunnel, socksConn net.Conn) {

	targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5(socksConn)
	if err != nil {
		fmt.Println("Socks5 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	protocol := "TCP"
	var rawTaskData []byte
	if socksCommand == 3 {
		rawTaskData = tunnel.handlerConnectUDP(channelId, targetAddress, targetPort)
		protocol = "UDP"
	} else {
		rawTaskData = tunnel.handlerConnectTCP(channelId, targetAddress, targetPort)
	}
	sendTunnelTaskData(agent, rawTaskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
		protocol:  protocol,
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handleRequestSocks5Auth(agent *Agent, tunnel *Tunnel, socksConn net.Conn, username string, password string) {

	targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5Auth(socksConn, username, password)
	if err != nil {
		fmt.Println("Socks5 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	protocol := "TCP"
	var rawTaskData []byte
	if socksCommand == 3 {
		rawTaskData = tunnel.handlerConnectUDP(channelId, targetAddress, targetPort)
		protocol = "UDP"
	} else {
		rawTaskData = tunnel.handlerConnectTCP(channelId, targetAddress, targetPort)
	}
	sendTunnelTaskData(agent, rawTaskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
		protocol:  protocol,
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handleLocalPortFwd(agent *Agent, tunnel *Tunnel, fwdConn net.Conn, fhost string, fport int) {
	channelId := int(rand.Uint32())
	rawTaskData := tunnel.handlerConnectTCP(channelId, fhost, fport)
	sendTunnelTaskData(agent, rawTaskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      fwdConn,
		protocol:  "TCP",
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handlerReverseAccept(agent *Agent, tunnel *Tunnel, channelId int) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		rawTaskData := tunnel.handlerClose(channelId)
		sendTunnelTaskData(agent, rawTaskData)
		return
	}

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      fwdConn,
		protocol:  "TCP",
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)

	go socketToTunnelData(agent, tunnel, tunnelConnection)
}

/// process socket

func sendTunnelTaskData(agent *Agent, rawTaskData []byte) {
	var (
		taskData adaptix.TaskData
		err      error
	)

	err = json.Unmarshal(rawTaskData, &taskData)
	if err != nil {
		return
	}

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id
	agent.TunnelQueue.Put(taskData)
}

func socketToTunnelData(agent *Agent, tunnel *Tunnel, tunnelConnection *TunnelConnection) {
	var rawTaskData []byte

	buffer := make([]byte, 0x10000)
	for {
		select {
		case <-tunnelConnection.ctx.Done():
			return
		default:
			n, err := tunnelConnection.conn.Read(buffer)
			if err != nil {
				if err == io.EOF {
					rawTaskData = tunnel.handlerClose(tunnelConnection.channelId)
				} else {
					fmt.Printf("Error read data: %v\n", err)
					continue
				}
				break
			} else {
				if tunnelConnection.protocol == "UDP" {
					rawTaskData = tunnel.handlerWriteUDP(tunnelConnection.channelId, buffer[:n])
				} else {
					rawTaskData = tunnel.handlerWriteTCP(tunnelConnection.channelId, buffer[:n])
				}
			}

			sendTunnelTaskData(agent, rawTaskData)
		}
	}
}

func tunnelDataToSocket(tunnelConnection *TunnelConnection, data []byte) {
	_, _ = tunnelConnection.conn.Write(data)
}
