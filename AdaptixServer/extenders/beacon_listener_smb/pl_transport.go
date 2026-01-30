package main

import (
	"encoding/json"
	"errors"
	"regexp"
)

type Listener struct {
	transport *TransportSMB
}

type TransportSMB struct {
	Config TransportConfig
	Name   string
	Active bool
}

type TransportConfig struct {
	Pipename   string `json:"pipename"`
	EncryptKey string `json:"encrypt_key"`
	Protocol   string `json:"protocol"`
}

func validConfig(config string) error {
	var conf TransportConfig
	err := json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return err
	}

	matched, err := regexp.MatchString(`^[a-zA-Z0-9\-_.]+$`, conf.Pipename)
	if err != nil || !matched {
		return errors.New("Pipename invalid")
	}

	match, _ := regexp.MatchString("^[0-9a-f]{32}$", conf.EncryptKey)
	if len(conf.EncryptKey) != 32 || !match {
		return errors.New("encrypt_key must be 32 hex characters")
	}

	return nil
}
