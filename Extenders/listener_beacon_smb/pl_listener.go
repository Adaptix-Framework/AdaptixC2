package main

import (
	"bytes"
	"crypto/rand"
	"crypto/rc4"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
)

const (
	SetType     = INTERNAL
	SetProtocol = "smb"
	SetName     = "BeaconSMB"
	SetUiPath   = "_ui_listener.json"
)

func (m *ModuleExtender) HandlerListenerValid(data string) error {

	/// START CODE HERE

	var (
		err  error
		conf SMBConfig
	)

	err = json.Unmarshal([]byte(data), &conf)
	if err != nil {
		return err
	}

	matched, err := regexp.MatchString(`^[a-zA-Z0-9\-_]+$`, conf.Pipename)
	if err != nil || !matched {
		return errors.New("uri invalid")
	}

	/// END CODE

	return nil
}

func (m *ModuleExtender) HandlerCreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (ListenerData, []byte, any, error) {
	var (
		listenerData ListenerData
		customdData  []byte
	)

	/// START CODE HERE

	var (
		listener *SMB
		conf     SMBConfig
		err      error
	)

	if listenerCustomData == nil {
		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		randSlice := make([]byte, 16)
		_, _ = rand.Read(randSlice)
		conf.EncryptKey = randSlice[:16]
		conf.Protocol = "smb"
	} else {
		err = json.Unmarshal(listenerCustomData, &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}
	}

	listener = &SMB{
		Name:   name,
		Config: conf,
		Active: true,
	}

	listenerData = ListenerData{
		BindHost:  "",
		BindPort:  "",
		AgentHost: "",
		AgentPort: listener.Config.Pipename,
		Status:    "Listen",
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

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (ListenerData, []byte, bool) {
	var (
		listenerData ListenerData
		customdData  []byte
		ok           bool = false
	)

	/// START CODE HERE

	var (
		err  error
		conf SMBConfig
	)

	listener := listenerObject.(*SMB)
	if listener.Name == name {

		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, false
		}

		listenerData = ListenerData{
			BindHost:  "",
			BindPort:  "",
			AgentHost: "",
			AgentPort: listener.Config.Pipename,
			Status:    "Listen",
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

	listener := listenerObject.(*SMB)
	if listener.Name == name {
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

	listener := listenerObject.(*SMB)
	if listener.Name == name {
		_ = json.NewEncoder(&object).Encode(listener.Config)
		ok = true
	}

	/// END CODE

	return object.Bytes(), ok
}

func (m *ModuleExtender) HandlerListenerInteralHandler(name string, data []byte, listenerObject any) (string, error, bool) {
	var (
		agentType string
		agentId   string
		agentInfo []byte
		err       error = nil
		ok        bool  = false
	)

	/// START CODE HERE

	listener := listenerObject.(*SMB)
	if listener.Name == name {

		rc4crypt, errcrypt := rc4.NewCipher(listener.Config.EncryptKey)
		if errcrypt != nil {
			return "", errcrypt, true
		}

		agentInfo = make([]byte, len(data))
		rc4crypt.XORKeyStream(agentInfo, data)

		agentType = fmt.Sprintf("%08x", uint(binary.BigEndian.Uint32(agentInfo[:4])))
		agentInfo = agentInfo[4:]
		agentId = fmt.Sprintf("%08x", uint(binary.BigEndian.Uint32(agentInfo[:4])))
		agentInfo = agentInfo[4:]
		
		if !ModuleObject.ts.TsAgentIsExists(agentId) {
			err = ModuleObject.ts.TsAgentCreate(agentType, agentId, agentInfo, listener.Name, "", false)
			if err != nil {
				return agentId, err, ok
			}
		}

		ok = true
	}

	/// END CODE

	return agentId, err, ok
}
