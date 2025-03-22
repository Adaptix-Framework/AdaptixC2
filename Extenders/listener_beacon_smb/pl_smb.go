package main

type SMBConfig struct {
	Pipename   string `json:"pipename"`
	Protocol   string `json:"protocol"`
	EncryptKey []byte `json:"encrypt_key"`
}

type SMB struct {
	Config SMBConfig
	Name   string
	Active bool
}
