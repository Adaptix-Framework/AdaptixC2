package main

type TCPConfig struct {
	Port    int    `json:"port_bind"`
	Prepend string `json:"prepend_data"`

	Protocol         string `json:"protocol"`
	EncryptKey       []byte `json:"-"`
	EncryptKeyHex    string `json:"encrypt_key_hex"`
	EncryptKeyBase64 string `json:"encrypt_key_base64"`
}

type TCP struct {
	Config TCPConfig
	Name   string
	Active bool
}
