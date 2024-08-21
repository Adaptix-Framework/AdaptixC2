package httphandler

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/token"
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

type Credentials struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

func (th *TsHttpHandler) login(ctx *gin.Context) {
	var creds Credentials
	if err := ctx.ShouldBindJSON(&creds); err != nil {
		_ = ctx.Error(errors.New("invalid credentials"))
		return
	}

	recvHash := krypt.SHA256([]byte(creds.Password))

	if recvHash != th.Hash {
		_ = ctx.Error(errors.New("incorrect password"))
		return
	}

	accessToken, err := token.GenerateAccessToken(creds.Username)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate access token"))
		return
	}

	refreshToken, err := token.GenerateRefreshToken(creds.Username)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate refresh token"))
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"access_token": accessToken, "refresh_token": refreshToken})
}
