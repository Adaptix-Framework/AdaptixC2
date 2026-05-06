package connector

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcServiceCall(ctx *gin.Context) {
	var jsonData struct {
		ServiceName string `json:"service"`
		Command     string `json:"command"`
		Args        string `json:"args"`
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err := ctx.ShouldBindJSON(&jsonData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	if jsonData.ServiceName == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": "service_name is required"})
		return
	}
	
	go tc.teamserver.TsServiceCall(jsonData.ServiceName, username, jsonData.Command, jsonData.Args)

	ctx.JSON(http.StatusOK, gin.H{"message": "success", "result": "ok"})
}

func (tc *TsConnector) TcServiceList(c *gin.Context) {
	services, err := tc.teamserver.TsServiceList()
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"message": "success", "services": services})
}

func (tc *TsConnector) TcServiceLoad(c *gin.Context) {
	var jsonData struct {
		ConfigPath string `json:"config_path"`
	}

	err := c.ShouldBindJSON(&jsonData)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	if jsonData.ConfigPath == "" {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": "config_path is required"})
		return
	}

	err = tc.teamserver.TsServiceLoad(jsonData.ConfigPath)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "success"})
}

func (tc *TsConnector) TcServiceUnload(c *gin.Context) {
	var jsonData struct {
		ServiceName string `json:"service"`
	}

	err := c.ShouldBindJSON(&jsonData)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	if jsonData.ServiceName == "" {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": "service is required"})
		return
	}

	err = tc.teamserver.TsServiceUnload(jsonData.ServiceName)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "success"})
}
