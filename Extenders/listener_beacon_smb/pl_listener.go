package main

import (
	"bytes"
	"crypto/rand"
	"crypto/rc4"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"

	adaptix "github.com/Adaptix-Framework/axc2"
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

	matched, err := regexp.MatchString(`^[a-zA-Z0-9\-_.]+$`, conf.Pipename)
	if err != nil || !matched {
		return errors.New("Pipename invalid")
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
		listener *SMB
		conf     SMBConfig
		err      error
	)

	if listenerCustomData == nil {
		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		// 处理自定义加密密钥
		if conf.EncryptKeyHex != "" {
			// 用户提供了自定义密钥，直接处理字符串
			keyBytes := []byte(conf.EncryptKeyHex)
			if len(keyBytes) > 16 {
				keyBytes = keyBytes[:16] // 截断到16字节
			} else if len(keyBytes) < 16 {
				// 填充到16字节
				padded := make([]byte, 16)
				copy(padded, keyBytes)
				keyBytes = padded
			}
			conf.EncryptKey = keyBytes
			// 设置Base64编码的密钥用于agent生成
			conf.EncryptKeyBase64 = base64.StdEncoding.EncodeToString(keyBytes)
		} else {
			// 生成随机密钥
			randSlice := make([]byte, 16)
			_, _ = rand.Read(randSlice)
			conf.EncryptKey = randSlice[:16]
			conf.EncryptKeyBase64 = base64.StdEncoding.EncodeToString(conf.EncryptKey)
		}
		conf.Protocol = "bind_smb"
	} else {
		err = json.Unmarshal(listenerCustomData, &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		// 从数据库恢复时，需要重新设置EncryptKey
		if conf.EncryptKeyBase64 != "" {
			conf.EncryptKey, err = base64.StdEncoding.DecodeString(conf.EncryptKeyBase64)
			if err != nil {
				return listenerData, customdData, listener, err
			}
		} else if conf.EncryptKeyHex != "" {
			// 兼容旧版本，从hex字段恢复
			keyBytes := []byte(conf.EncryptKeyHex)
			if len(keyBytes) > 16 {
				keyBytes = keyBytes[:16]
			} else if len(keyBytes) < 16 {
				padded := make([]byte, 16)
				copy(padded, keyBytes)
				keyBytes = padded
			}
			conf.EncryptKey = keyBytes
			conf.EncryptKeyBase64 = base64.StdEncoding.EncodeToString(keyBytes)
		}
	}

	listener = &SMB{
		Name:   name,
		Config: conf,
		Active: true,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  "",
		BindPort:  "",
		AgentAddr: "\\\\.\\pipe\\" + listener.Config.Pipename,
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

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (adaptix.ListenerData, []byte, bool) {
	var (
		listenerData adaptix.ListenerData
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

		// 更新监听器配置
		listener.Config.Pipename = conf.Pipename

		// 更新加密密钥
		if conf.EncryptKeyHex != "" {
			keyBytes := []byte(conf.EncryptKeyHex)
			if len(keyBytes) > 16 {
				keyBytes = keyBytes[:16]
			} else if len(keyBytes) < 16 {
				padded := make([]byte, 16)
				copy(padded, keyBytes)
				keyBytes = padded
			}
			listener.Config.EncryptKey = keyBytes
			listener.Config.EncryptKeyHex = conf.EncryptKeyHex
			listener.Config.EncryptKeyBase64 = base64.StdEncoding.EncodeToString(keyBytes)
		}

		listenerData = adaptix.ListenerData{
			BindHost:  "",
			BindPort:  "",
			AgentAddr: "\\\\.\\pipe\\" + listener.Config.Pipename,
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
			_, err = ModuleObject.ts.TsAgentCreate(agentType, agentId, agentInfo, listener.Name, "", false)
			if err != nil {
				return agentId, err, ok
			}
		}

		ok = true
	}

	/// END CODE

	return agentId, err, ok
}
