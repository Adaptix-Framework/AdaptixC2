package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"regexp"
	"strconv"
	"strings"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gin-gonic/gin"
)

func (m *ModuleExtender) HandlerListenerValid(data string) error {

	/// START CODE HERE

	var (
		err  error
		conf HTTPConfig
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

	matched, err := regexp.MatchString(`^/[a-zA-Z0-9\.\=\-]+(/[a-zA-Z0-9\.\=\-]+)*$`, conf.Uri)
	if err != nil || !matched {
		return errors.New("uri invalid")
	}

	if conf.HttpMethod == "" {
		return errors.New("http_method is required")
	}

	if conf.ParameterName == "" {
		return errors.New("hb_header is required")
	}

	if conf.UserAgent == "" {
		return errors.New("user_agent is required")
	}

	// Check if custom key is enabled
	if conf.UseCustomKey {
		// Validate custom key length (6-32 characters)
		if len(conf.EncryptKey) < 6 || len(conf.EncryptKey) > 32 {
			return errors.New("encrypt_key must be between 6 and 32 characters when custom key is enabled")
		}
	} else {
		// Original validation for hex key
		match, _ := regexp.MatchString("^[0-9a-f]{32}$", conf.EncryptKey)
		if len(conf.EncryptKey) != 32 || !match {
			return errors.New("encrypt_key must be 32 hex characters")
		}
	}

	if !strings.Contains(conf.WebPageOutput, "<<<PAYLOAD_DATA>>>") {
		return errors.New("page-payload must contain '<<<PAYLOAD_DATA>>>' template")
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
		listener *HTTP
		conf     HTTPConfig
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

		conf.RequestHeaders = strings.TrimRight(conf.RequestHeaders, " \n\t\r") + "\n"
		conf.RequestHeaders = strings.ReplaceAll(conf.RequestHeaders, "\n", "\r\n")
		if len(conf.HostHeader) > 0 {
			conf.RequestHeaders = fmt.Sprintf("Host: %s\r\n%s", conf.HostHeader, conf.RequestHeaders)
		}

		conf.ResponseHeaders = make(map[string]string)
		headerLine := strings.Split(conf.Server_headers, "\n")
		for _, line := range headerLine {
			line = strings.TrimSpace(line)
			if line == "" {
				continue
			}

			parts := strings.SplitN(line, ":", 2)
			if len(parts) != 2 {
				continue
			}
			key := strings.TrimSpace(parts[0])
			value := strings.TrimSpace(parts[1])

			conf.ResponseHeaders[key] = value
		}
		conf.Protocol = "http"

	} else {
		err = json.Unmarshal(listenerCustomData, &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}
	}

	listener = &HTTP{
		GinEngine: gin.New(),
		Name:      name,
		Config:    conf,
		Active:    false,
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

	if listener.Config.Ssl {
		listenerData.Protocol = "https"
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
		conf HTTPConfig
	)

	listener := listenerObject.(*HTTP)
	if listener.Name == name {

		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, false
		}

		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, " ", "")
		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, "\n", ", ")
		conf.Callback_addresses = strings.TrimSuffix(conf.Callback_addresses, ", ")

		conf.RequestHeaders = strings.TrimRight(conf.RequestHeaders, " \n\t\r") + "\n"
		conf.RequestHeaders = strings.ReplaceAll(conf.RequestHeaders, "\n", "\r\n")
		if len(conf.HostHeader) > 0 {
			conf.RequestHeaders = fmt.Sprintf("Host: %s\r\n%s", conf.HostHeader, conf.RequestHeaders)
		}

		listener.Config.Callback_addresses = conf.Callback_addresses

		listener.Config.UserAgent = conf.UserAgent
		listener.Config.Uri = conf.Uri
		listener.Config.ParameterName = conf.ParameterName
		listener.Config.TrustXForwardedFor = conf.TrustXForwardedFor
		listener.Config.HostHeader = conf.HostHeader
		listener.Config.RequestHeaders = conf.RequestHeaders
		listener.Config.WebPageError = conf.WebPageError
		listener.Config.WebPageOutput = conf.WebPageOutput

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

	listener := listenerObject.(*HTTP)
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

	listener := listenerObject.(*HTTP)
	if listener.Name == name {
		_ = json.NewEncoder(&object).Encode(listener.Config)
		ok = true
	}

	/// END CODE

	return object.Bytes(), ok
}
