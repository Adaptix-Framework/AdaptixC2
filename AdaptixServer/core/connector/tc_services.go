package connector

import (
	"encoding/json"
	"net/http"

	adaptix "github.com/Adaptix-Framework/axc2/v2"
	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcPluginServiceCall(ctx *gin.Context) {
	var jsonData struct {
		ServiceName string `json:"service"`
		Command     string `json:"command"`
		Args        string `json:"args"`
	}

	username, ok := mustUsername(ctx)
	if !ok {
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

	go tc.teamserver.TsPluginServiceCall(jsonData.ServiceName, username, jsonData.Command, jsonData.Args)

	ctx.JSON(http.StatusOK, gin.H{"message": "success", "result": "ok"})
}

func (tc *TsConnector) TcPluginServiceCallWait(ctx *gin.Context) {
	var jsonData struct {
		ServiceName string `json:"service"`
		Command     string `json:"command"`
		Args        string `json:"args"`
		TimeoutMs   int    `json:"timeout_ms"`
	}

	username, ok := mustUsername(ctx)
	if !ok {
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

	result, err := tc.teamserver.TsPluginServiceCallWait(jsonData.ServiceName, username, jsonData.Command, jsonData.Args, jsonData.TimeoutMs)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "success", "result": result})
}

func (tc *TsConnector) TcServiceCatalog(c *gin.Context) {
	raw, err := tc.teamserver.TsServiceCatalog()
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}
	c.Data(http.StatusOK, "application/json; charset=utf-8", []byte(raw))
}

func (tc *TsConnector) TcServiceCatalogOne(c *gin.Context) {
	name := c.Param("name")
	raw, err := tc.teamserver.TsServiceCatalog()
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}
	var items []adaptix.ServiceCatalogItem
	if err := json.Unmarshal([]byte(raw), &items); err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}
	for _, it := range items {
		if it.Name == name {
			c.JSON(http.StatusOK, it)
			return
		}
	}
	c.JSON(http.StatusOK, gin.H{"message": "error", "error": "service not found"})
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
