package profile

type AdaptixProfile struct {
	Server *TsProfile `json:"Teamserver"`
}

type TsProfile struct {
	Port     int    `json:"port"`
	Endpoint string `json:"endpoint"`
	Password string `json:"password"`
	Cert     string `json:"cert"`
	Key      string `json:"key"`
}
