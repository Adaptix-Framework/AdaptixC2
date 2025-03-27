package profile

import (
	"AdaptixServer/core/utils/logs"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"os"
)

func NewProfile() *AdaptixProfile {
	return new(AdaptixProfile)
}

func (p *AdaptixProfile) IsValid() error {
	valid := true

	if p.Server.Port < 1 || 65535 < p.Server.Port {
		logs.Error("", "'Teamserver.port' must be between 1 and 65535 (current is %v)", p.Server.Port)
		valid = false
	}

	if isvalid.ValidUriString(p.Server.Endpoint) == false {
		logs.Error("'Teamserver.endpoint' must be valid URI value (current is %v)", p.Server.Endpoint)
		valid = false
	}

	if p.Server.Password == "" {
		logs.Error("", "'Teamserver.password' must be set")
		valid = false
	}

	if p.Server.Cert == "" {
		logs.Error("", "'Teamserver.cert' must be set")
		valid = false
	} else {
		_, err := os.Stat(p.Server.Cert)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("", "'Teamserver.cert': file does not exists")
				valid = false
			}
		}
	}

	if p.Server.Key == "" {
		logs.Error("", "'Teamserver.key' must be set")
		valid = false
	} else {
		_, err := os.Stat(p.Server.Key)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("", "'Teamserver.key': file does not exists")
				valid = false
			}
		}
	}

	if p.Server.Extenders != nil {
		for _, ext := range p.Server.Extenders {
			if ext != "" {
				_, err := os.Stat(ext)
				if err != nil {
					if os.IsNotExist(err) {
						logs.Error("", "Extender %s: file does not exists", ext)
						valid = false
					}
				}
			}
		}
	}

	if p.ServerResponse.PagePath != "" {
		_, err := os.Stat(p.ServerResponse.PagePath)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("", "'ServerResponse.page': file does not exists")
				valid = false
			}
		}
	}

	if valid {
		return nil
	} else {
		return errors.New("Invalid profile\n")
	}
}
