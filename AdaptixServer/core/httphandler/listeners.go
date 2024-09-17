package httphandler

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

type ListenerConfig struct {
	ConfigName string `json:"name"`
	ConfigType string `json:"type"`
	Config     string `json:"config"`
}

func (th *TsHttpHandler) ListenerStart(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"answer": "Listener started"})
}
