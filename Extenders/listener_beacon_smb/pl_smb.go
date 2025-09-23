package main

type SMBConfig struct {
	Pipename   string `json:"pipename"`
	EncryptKey string `json:"encrypt_key"`
	Protocol   string `json:"protocol"`
}

type SMB struct {
	Config SMBConfig
	Name   string
	Active bool
}
