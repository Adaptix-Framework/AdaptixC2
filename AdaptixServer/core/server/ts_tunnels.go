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

func (ts *Teamserver) TsTunnelStop(TunnelId string) {
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.listener.Close()

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}

/// Socks5

func (ts *Teamserver) TsTunnelStartSocks5(AgentId string, Port int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) error {
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
	addr := "0.0.0.0:" + port
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

	id := krypt.CRC32([]byte(agent.Data.Id + "socks5" + port))
	tunnelId := fmt.Sprintf("%08x", id)

	socksTunnel.Data = adaptix.TunnelData{
		TunnelId: tunnelId,
		AgentId:  agent.Data.Id,
		Computer: agent.Data.Computer,
		Username: agent.Data.Username,
		Process:  agent.Data.Process,
		Type:     "SOCKS5 proxy",
		Info:     "",
		Port:     port,
		Client:   "",
		Fport:    "",
		Fhost:    "",
	}

	ts.tunnels.Put(tunnelId, socksTunnel)

	packet := CreateSpTunnelCreate(socksTunnel.Data)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks5" + port))
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
