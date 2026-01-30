package main

import (
	"encoding/json"
	"errors"
	"regexp"
)

type Listener struct {
	transport *TransportTCP
}

type TransportConfig struct {
	Port       int    `json:"port_bind"`
	Prepend    string `json:"prepend_data"`
	EncryptKey string `json:"encrypt_key"`

	Protocol string `json:"protocol"`
}

type TransportTCP struct {
	Config TransportConfig
	Name   string
	Active bool
}

func validConfig(config string) error {
	var conf TransportConfig
	err := json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return err
	}

	if conf.Port < 1 || conf.Port > 65535 {
		return errors.New("Port must be in the range 1-65535")
	}

	match, _ := regexp.MatchString("^[0-9a-f]{32}$", conf.EncryptKey)
	if len(conf.EncryptKey) != 32 || !match {
		return errors.New("encrypt_key must be 32 hex characters")
	}

	return nil
}
