package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
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

func (ts *Teamserver) TsTunnelCreate(tunnel *Tunnel) {

	ts.tunnels.Put(tunnel.Data.TunnelId, tunnel)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	message := ""
	if tunnel.Data.Type == "SOCKS5 proxy" {
		message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS4 proxy" {
		message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS5 Auth proxy" {
		message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "Local port forward" {
		message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Data.Type == "Remote port forward" {
		message = fmt.Sprintf("Remote port forward to '%s:%s'", tunnel.Data.Fhost, tunnel.Data.Fport)
	}

	packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)
}

func (ts *Teamserver) TsTunnelStop(TunnelId string) error {
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return errors.New("Tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.listener.Close()

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClients(packet)

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
		return errors.New("Tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.Data.Info = Info

	packet := CreateSpTunnelEdit(tunnel.Data)
	ts.TsSyncAllClients(packet)

	return nil
}

/// Socks5

func (ts *Teamserver) TsTunnelStartSocks4(AgentId string, Address string, Port int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) error {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnect: FuncMsgConnect,
		handlerWrite:   FuncMsgWrite,
		handlerClose:   FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				fmt.Printf("Error socks connect: %v\n", err)
				return
			}
			go handleRequestSocks4(agent, socksTunnel, conn)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return err
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

	ts.TsTunnelCreate(socksTunnel)

	return nil
}

func (ts *Teamserver) TsTunnelStartSocks5(AgentId string, Address string, Port int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) error {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnect: FuncMsgConnect,
		handlerWrite:   FuncMsgWrite,
		handlerClose:   FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				fmt.Printf("Error socks connect: %v\n", err)
				return
			}
			go handleRequestSocks5(agent, socksTunnel, conn)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return err
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

	ts.TsTunnelCreate(socksTunnel)

	return nil
}

func (ts *Teamserver) TsTunnelStartSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) error {
	var (
		agent       *Agent
		socksTunnel *Tunnel
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ = value.(*Agent)

	port := strconv.Itoa(Port)
	addr := Address + ":" + port
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	socksTunnel = &Tunnel{
		listener:    listener,
		connections: safe.NewMap(),

		handlerConnect: FuncMsgConnect,
		handlerWrite:   FuncMsgWrite,
		handlerClose:   FuncMsgClose,
	}

	go func() {
		for {
			var conn net.Conn
			conn, err = socksTunnel.listener.Accept()
			if err != nil {
				fmt.Printf("Error socks connect: %v\n", err)
				return
			}
			go handleRequestSocks5Auth(agent, socksTunnel, conn, Username, Password)
		}
	}()

	time.Sleep(300 * time.Millisecond)

	if err != nil {
		return err
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

	ts.TsTunnelCreate(socksTunnel)

	return nil
}

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	ts.TsTunnelStop(TunnelId)
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
		tunnelConnection.conn.Close()
		tunnel.connections.Delete(strconv.Itoa(channelId))
	}
}

/// handlers

func handleRequestSocks4(agent *Agent, tunnel *Tunnel, socksConn net.Conn) {

	targetAddress, targetPort, err := proxy.CheckSocks4(socksConn)
	if err != nil {
		fmt.Println("Socks4 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	rawTaskData := tunnel.handlerConnect(channelId, targetAddress, targetPort)

	var taskData adaptix.TaskData
	err = json.Unmarshal(rawTaskData, &taskData)
	if err != nil {
		return
	}

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id
	agent.TunnelQueue.Put(taskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handleRequestSocks5(agent *Agent, tunnel *Tunnel, socksConn net.Conn) {

	targetAddress, targetPort, err := proxy.CheckSocks5(socksConn)
	if err != nil {
		fmt.Println("Socks5 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	rawTaskData := tunnel.handlerConnect(channelId, targetAddress, targetPort)

	var taskData adaptix.TaskData
	err = json.Unmarshal(rawTaskData, &taskData)
	if err != nil {
		return
	}

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id
	agent.TunnelQueue.Put(taskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}

func handleRequestSocks5Auth(agent *Agent, tunnel *Tunnel, socksConn net.Conn, username string, password string) {

	targetAddress, targetPort, err := proxy.CheckSocks5Auth(socksConn, username, password)
	if err != nil {
		fmt.Println("Socks5 proxy error: ", err)
		return
	}

	channelId := int(rand.Uint32())
	rawTaskData := tunnel.handlerConnect(channelId, targetAddress, targetPort)

	var taskData adaptix.TaskData
	err = json.Unmarshal(rawTaskData, &taskData)
	if err != nil {
		return
	}

	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id
	agent.TunnelQueue.Put(taskData)

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      socksConn,
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)
}


/// process socket

func socketToTunnelData(agent *Agent, tunnel *Tunnel, tunnelConnection *TunnelConnection) {
	var rawTaskData []byte

	buffer := make([]byte, 0x10000)
	for {
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
			rawTaskData = tunnel.handlerWrite(tunnelConnection.channelId, buffer[:n])
		}

		var taskData adaptix.TaskData
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
}

func tunnelDataToSocket(tunnelConnection *TunnelConnection, data []byte) {
	tunnelConnection.conn.Write(data)
}
