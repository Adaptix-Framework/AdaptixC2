package profile

type AdaptixProfile struct {
	Server         *TsProfile  `json:"Teamserver"`
	ServerResponse *TsResponse `json:"ServerResponse"`
}

type TsProfile struct {
	Port     int    `json:"port"`
	Endpoint string `json:"endpoint"`
	Password string `json:"password"`
	Cert     string `json:"cert"`
	Key      string `json:"key"`
	Ext      string `json:"extender"`
}

type TsResponse struct {
	Status      int               `json:"status"`
	Headers     map[string]string `json:"headers"`
	PagePath    string            `json:"page"`
	PageContent string
}
