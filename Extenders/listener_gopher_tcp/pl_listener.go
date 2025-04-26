package main

import (
	"bytes"
	"crypto/rand"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"net"
	"strconv"
	"strings"
)

func (m *ModuleExtender) HandlerListenerValid(data string) error {

	/// START CODE HERE

	var (
		err  error
		conf TCPConfig
	)

	err = json.Unmarshal([]byte(data), &conf)
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

	/// END CODE

	return nil
}

func (m *ModuleExtender) HandlerCreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (adaptix.ListenerData, []byte, any, error) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
	)

	/// START CODE HERE

	var (
		listener *TCP
		conf     TCPConfig
		err      error
	)

	if listenerCustomData == nil {
		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, " ", "")
		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, "\n", ", ")
		conf.Callback_addresses = strings.TrimSuffix(conf.Callback_addresses, ", ")

		randSlice := make([]byte, 16)
		_, _ = rand.Read(randSlice)
		conf.EncryptKey = randSlice[:16]
		conf.Protocol = "tcp"

	} else {
		err = json.Unmarshal(listenerCustomData, &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}
	}

	listener = &TCP{
		Name:          name,
		Config:        conf,
		AgentConnects: NewMap(),
		JobConnects:   NewMap(),
	}

	err = listener.Start(m.ts)
	if err != nil {
		return listenerData, customdData, listener, err
	}

	listenerData = adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: conf.Callback_addresses,
		Status:    "Listen",
	}

	if !listener.Active {
		listenerData.Status = "Closed"
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(listener.Config)
	if err != nil {
		return listenerData, customdData, listener, nil
	}
	customdData = buffer.Bytes()

	/// END CODE

	return listenerData, customdData, listener, nil
}

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (adaptix.ListenerData, []byte, bool) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
		ok           bool = false
	)

	/// START CODE HERE

	var (
		err  error
		conf TCPConfig
	)

	listener := listenerObject.(*TCP)
	if listener.Name == name {

		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, false
		}

		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, " ", "")
		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, "\n", ", ")
		conf.Callback_addresses = strings.TrimSuffix(conf.Callback_addresses, ", ")

		listener.Config.Callback_addresses = conf.Callback_addresses

		listener.Config.TcpBanner = conf.TcpBanner
		listener.Config.ErrorAnswer = conf.ErrorAnswer
		listener.Config.Timeout = conf.Timeout

		listenerData = adaptix.ListenerData{
			BindHost:  listener.Config.HostBind,
			BindPort:  strconv.Itoa(listener.Config.PortBind),
			AgentAddr: listener.Config.Callback_addresses,
			Status:    "Listen",
		}
		if !listener.Active {
			listenerData.Status = "Closed"
		}

		var buffer bytes.Buffer
		err = json.NewEncoder(&buffer).Encode(listener.Config)
		if err != nil {
			return listenerData, customdData, false
		}
		customdData = buffer.Bytes()

		ok = true
	}

	/// END CODE

	return listenerData, customdData, ok
}

func (m *ModuleExtender) HandlerListenerStop(name string, listenerObject any) (bool, error) {
	var (
		err error = nil
		ok  bool  = false
	)

	/// START CODE HERE

	listener := listenerObject.(*TCP)
	if listener.Name == name {
		err = listener.Stop()
		ok = true
	}

	/// END CODE

	return ok, err
}

func (m *ModuleExtender) HandlerListenerGetProfile(name string, listenerObject any) ([]byte, bool) {
	var (
		object bytes.Buffer
		ok     bool = false
	)

	/// START CODE HERE

	listener := listenerObject.(*TCP)
	if listener.Name == name {
		_ = json.NewEncoder(&object).Encode(listener.Config)
		ok = true
	}

	/// END CODE

	return object.Bytes(), ok
}
