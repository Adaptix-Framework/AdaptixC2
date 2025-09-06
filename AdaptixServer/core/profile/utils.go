package profile

type AdaptixProfile struct {
	Server         *TsProfile  `json:"Teamserver"`
	ServerResponse *TsResponse `json:"ServerResponse"`
	Callbacks      *TsCallback `json:"EventCallback"`
}

type TsProfile struct {
	Interface  string            `json:"interface"`
	Port       int               `json:"port"`
	Endpoint   string            `json:"endpoint"`
	Password   string            `json:"password"`
	Operators  map[string]string `json:"operators"`
	Cert       string            `json:"cert"`
	Key        string            `json:"key"`
	Extenders  []string          `json:"extenders"`
	ATokenLive int               `json:"access_token_live_hours"`
	RTokenLive int               `json:"refresh_token_live_hours"`
}

type TsResponse struct {
	Status      int               `json:"status"`
	Headers     map[string]string `json:"headers"`
	PagePath    string            `json:"page"`
	PageContent string
}

type TsCallback struct {
	Telegram struct {
		Token   string   `json:"token"`
		ChatsId []string `json:"chats_id"`
	} `json:"Telegram"`
	NewAgentMessage    string `json:"new_agent_message"`
	NewCredMessage     string `json:"new_cred_message"`
	NewDownloadMessage string `json:"new_download_message"`
}
