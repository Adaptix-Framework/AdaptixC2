package profile

type AdaptixProfile struct {
	Server     *TsProfile    `yaml:"Teamserver"`
	HttpServer *TsHttpServer `yaml:"HttpServer"`
	Callbacks  *TsCallback   `yaml:"EventCallback"`
}

type TsProfile struct {
	Interface    string            `yaml:"interface"`
	Port         int               `yaml:"port"`
	Endpoint     string            `yaml:"endpoint"`
	Password     string            `yaml:"password"`
	OnlyPassword bool              `yaml:"only_password"`
	Operators    map[string]string `yaml:"operators"`
	Cert         string            `yaml:"cert"`
	Key          string            `yaml:"key"`
	Extenders    []string          `yaml:"extenders"`
	ATokenLive   int               `yaml:"access_token_live_hours"`
	RTokenLive   int               `yaml:"refresh_token_live_hours"`
}

type TsHttpServer struct {
	Error *TsHttpError  `yaml:"error"`
	HTTP  *TsHTTPConfig `yaml:"http"`
	TLS   *TsTLSConfig  `yaml:"tls"`
}

type TsHttpError struct {
	Status      int               `yaml:"status"`
	Headers     map[string]string `yaml:"headers"`
	PagePath    string            `yaml:"page"`
	PageContent string
}

type TsHTTPConfig struct {
	MaxHeaderBytes        int    `yaml:"max_header_bytes"`
	ReadHeaderTimeoutSec  int    `yaml:"read_header_timeout_sec"`
	ReadTimeoutSec        int    `yaml:"read_timeout_sec"`
	WriteTimeoutSec       int    `yaml:"write_timeout_sec"`
	IdleTimeoutSec        int    `yaml:"idle_timeout_sec"`
	RequestTimeoutSec     int    `yaml:"request_timeout_sec"`
	RequestTimeoutMessage string `yaml:"request_timeout_message"`
	DisableKeepAlives     bool   `yaml:"disable_keep_alives"`
	EnableHTTP2           *bool  `yaml:"enable_http2"`
}

type TsTLSConfig struct {
	MinVersion               string   `yaml:"min_version"`
	MaxVersion               string   `yaml:"max_version"`
	PreferServerCipherSuites *bool    `yaml:"prefer_server_cipher_suites"`
	CipherSuites             []string `yaml:"cipher_suites"`
}

type WebhookConfig struct {
	URL     string            `yaml:"url"`
	Method  string            `yaml:"method"`
	Headers map[string]string `yaml:"headers"`
	Data    string            `yaml:"data"`
}

type TsCallback struct {
	Telegram struct {
		Token   string   `yaml:"token"`
		ChatsId []string `yaml:"chats_id"`
	} `yaml:"Telegram"`

	Slack struct {
		WebhookURL string `yaml:"webhook_url"`
	} `yaml:"Slack"`

	Webhooks []WebhookConfig `yaml:"Webhooks"`

	NewAgentMessage    string `yaml:"new_agent_message"`
	NewCredMessage     string `yaml:"new_cred_message"`
	NewDownloadMessage string `yaml:"new_download_message"`
}
