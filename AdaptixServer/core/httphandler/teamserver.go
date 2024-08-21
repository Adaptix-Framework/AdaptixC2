package httphandler

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"fmt"
	"github.com/gin-gonic/gin"
	"net/http"
)

type Teamserver interface {
}

type TsHttpHandler struct {
	Port     int
	Hash     string
	Endpoint string
	Cert     string
	Key      string

	Engine     *gin.Engine
	teamserver Teamserver
}

func default404Middleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Next()

		if len(c.Errors) > 0 && !c.Writer.Written() {
			c.String(http.StatusNotFound, "Default - Resource not found")
		}
	}
}

func NewTsHttpHandler(ts Teamserver, p profile.TsProfile) (*TsHttpHandler, error) {
	gin.SetMode(gin.ReleaseMode)

	var httpHandler = new(TsHttpHandler)
	httpHandler.Engine = gin.New()
	httpHandler.teamserver = ts
	httpHandler.Port = p.Port
	httpHandler.Endpoint = p.Endpoint
	httpHandler.Hash = krypt.SHA256([]byte(p.Password))
	httpHandler.Key = p.Key
	httpHandler.Cert = p.Cert

	httpHandler.Engine.POST(p.Endpoint+"/login", default404Middleware(), httpHandler.login)
	httpHandler.Engine.POST(p.Endpoint+"/refresh", default404Middleware(), token.RefreshTokenHandler)

	httpHandler.Engine.POST(p.Endpoint+"/check", default404Middleware(), token.ValidateAccessToken(), func(c *gin.Context) {
		username := c.GetString("username")
		c.JSON(http.StatusOK, gin.H{"user": username})
	})

	httpHandler.Engine.NoRoute(default404Middleware(), func(c *gin.Context) { _ = c.Error(errors.New("NoRoute")) })

	return httpHandler, nil
}

func (th *TsHttpHandler) Start(finished *chan bool) {
	host := fmt.Sprintf(":%d", th.Port)
	err := th.Engine.RunTLS(host, th.Cert, th.Key)
	if err != nil {
		logs.Error("Failed to start HTTP Server: " + err.Error())
		return
	}
	*finished <- true
}
