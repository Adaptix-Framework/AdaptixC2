package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/utils/krypt"
	"errors"
)

func (ts *Teamserver) CreateOTP(otpType string, data interface{}) (string, error) {
	var storeValue interface{}

	switch otpType {

	case "download":
		id, ok := data.(string)
		if !ok || id == "" {
			return "", errors.New("Invalid FileId")
		}
		_, err := ts.TsDownloadGet(id)
		if err != nil {
			return "", errors.New("Invalid FileId")
		}
		storeValue = id

	case "tmp_upload":
		id, ok := data.(string)
		if !ok || id == "" {
			return "", errors.New("Invalid FileId")
		}
		if ts.tmp_uploads.Contains(id) {
			return "", errors.New("Invalid FileId")
		}

		filename, err := krypt.GenerateUID(20)
		if err != nil {
			return "", err
		}
		filename += ".tmp"
		ts.tmp_uploads.Put(id, filename)
		storeValue = id

	case "connect":
		wsData, ok := data.(connector.ConnectOTPData)
		if !ok || wsData.Username == "" {
			return "", errors.New("invalid connect OTP data")
		}
		storeValue = wsData

	case "channel_tunnel", "channel_terminal", "channel_agent_build":
		wsData, ok := data.(*connector.ChannelOTPData)
		if !ok || wsData == nil || wsData.Username == "" {
			return "", errors.New("invalid channel OTP data")
		}
		storeValue = wsData

	default:
		return "", errors.New("invalid otpType")
	}

	return ts.OTPManager.Create(otpType, storeValue)
}

func (ts *Teamserver) ValidateOTP(token string) (string, interface{}, bool) {
	return ts.OTPManager.Validate(token)
}
