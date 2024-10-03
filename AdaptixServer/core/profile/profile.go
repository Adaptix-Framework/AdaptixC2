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

func (p *AdaptixProfile) IsVaid() error {
	valid := true

	if p.Server.Port < 1 || 65535 < p.Server.Port {
		logs.Error("'Teamserver.port' must be between 1 and 65535 (current is %v)", p.Server.Port)
		valid = false
	}

	if isvalid.ValidUriString(p.Server.Endpoint) == false {
		logs.Error("'Teamserver.endpoint' must be valid URI value (current is %v)", p.Server.Endpoint)
		valid = false
	}

	if p.Server.Password == "" {
		logs.Error("'Teamserver.password' must be set")
		valid = false
	}

	if p.Server.Cert == "" {
		logs.Error("'Teamserver.cert' must be set")
		valid = false
	} else {
		_, err := os.Stat(p.Server.Cert)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("'Teamserver.cert': file does not exists")
				valid = false
			}
		}
	}

	if p.Server.Key == "" {
		logs.Error("'Teamserver.key' must be set")
		valid = false
	} else {
		_, err := os.Stat(p.Server.Key)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("'Teamserver.key': file does not exists")
				valid = false
			}
		}
	}

	if p.Server.Ext != "" {
		_, err := os.Stat(p.Server.Ext)
		if err != nil {
			if os.IsNotExist(err) {
				logs.Error("'Teamserver.extender': file does not exists")
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
