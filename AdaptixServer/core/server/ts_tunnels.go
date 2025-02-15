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
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.listener.Close()
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
