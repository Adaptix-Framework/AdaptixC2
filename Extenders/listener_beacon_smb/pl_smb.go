package main

type SMBConfig struct {
	Pipename         string `json:"pipename"`
	Protocol         string `json:"protocol"`
	EncryptKey       []byte `json:"-"`
	EncryptKeyHex    string `json:"encrypt_key_hex"`
	EncryptKeyBase64 string `json:"encrypt_key_base64"`
}

type SMB struct {
	Config SMBConfig
	Name   string
	Active bool
}
