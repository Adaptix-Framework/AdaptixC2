package server

import (
	"io"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (ts *Teamserver) TsEndpointRegister(method string, path string, handler func(username string, body []byte) (int, []byte)) error {
	ginHandler := func(c *gin.Context) {
		username := c.GetString("username")

		var body []byte
		if c.Request.Body != nil {
			body, _ = io.ReadAll(c.Request.Body)
		}

		statusCode, responseBody := handler(username, body)
		c.Data(statusCode, "application/json", responseBody)
	}

	return ts.AdaptixServer.RegisterEndpoint(method, path, ginHandler)
}

func (ts *Teamserver) TsEndpointRegisterRaw(method string, path string, handler func(w http.ResponseWriter, r *http.Request, username string)) error {
	ginHandler := func(c *gin.Context) {
		username := c.GetString("username")
		handler(c.Writer, c.Request, username)
	}

	return ts.AdaptixServer.RegisterEndpoint(method, path, ginHandler)
}

func (ts *Teamserver) TsEndpointUnregister(method string, path string) error {
	return ts.AdaptixServer.UnregisterEndpoint(method, path)
}

func (ts *Teamserver) TsEndpointExists(method string, path string) bool {
	return ts.AdaptixServer.EndpointExists(method, path)
}

func (ts *Teamserver) TsEndpointRegisterPublic(method string, path string, handler func(body []byte) (int, []byte)) error {
	ginHandler := func(c *gin.Context) {
		var body []byte
		if c.Request.Body != nil {
			body, _ = io.ReadAll(c.Request.Body)
		}

		statusCode, responseBody := handler(body)
		c.Data(statusCode, "application/json", responseBody)
	}

	return ts.AdaptixServer.RegisterPublicEndpoint(method, path, ginHandler)
}

func (ts *Teamserver) TsEndpointRegisterPublicRaw(method string, path string, handler func(w http.ResponseWriter, r *http.Request)) error {
	ginHandler := func(c *gin.Context) {
		handler(c.Writer, c.Request)
	}

	return ts.AdaptixServer.RegisterPublicEndpoint(method, path, ginHandler)
}

func (ts *Teamserver) TsEndpointUnregisterPublic(method string, path string) error {
	return ts.AdaptixServer.UnregisterPublicEndpoint(method, path)
}

func (ts *Teamserver) TsEndpointExistsPublic(method string, path string) bool {
	return ts.AdaptixServer.PublicEndpointExists(method, path)
}
