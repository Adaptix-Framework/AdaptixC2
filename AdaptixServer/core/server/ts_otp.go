package server

import (
	"AdaptixServer/core/utils/krypt"
	"errors"

	"github.com/gin-gonic/gin"
)

func (ts *Teamserver) CreateOTP(otpType string, id string) (string, error) {
	switch otpType {

	case "download":
		if !ts.downloads.Contains(id) {
			return "", errors.New("Invalid FileId")
		}

	case "tmp_upload":
		if ts.tmp_uploads.Contains(id) {
			return "", errors.New("Invalid FileId")
		}

		filename, err := krypt.GenerateUID(12)
		if err != nil {
			return "", err
		}
		filename += ".tmp"
		ts.tmp_uploads.Put(id, filename)

	default:
		return "", errors.New("invalid otpType")
	}

	otp, err := krypt.GenerateUID(24)
	if err != nil {
		return "", err
	}

	otp += id
	ts.otps.Put(otp, id)
	return otp, nil
}

func (ts *Teamserver) ValidateOTP() gin.HandlerFunc {
	return func(ctx *gin.Context) {
		otp := ctx.GetHeader("OTP")
		if otp == "" {
			_ = ctx.Error(errors.New("authorization token required"))
			return
		}

		value, ok := ts.otps.GetDelete(otp)
		if !ok {
			_ = ctx.Error(errors.New("authorization token required"))
			return
		}
		objectId, _ := value.(string)

		ctx.Set("objectId", objectId)
		ctx.Set("otp", otp)
		ctx.Next()
	}
}
