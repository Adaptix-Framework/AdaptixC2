package main

import (
	"flag"
	"os"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/mcp"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

func main() {
	// 命令行参数
	clientURL := flag.String("url", "ws://127.0.0.1:9999", "Client MCP Bridge URL")
	flag.Parse()

	utils.InfoLogger.Println("🚀 Starting AdaptixC2 MCP Server...")
	utils.InfoLogger.Printf("📡 Client URL: %s", *clientURL)

	// 创建Client连接器
	connector := client.NewConnector(*clientURL)

	// 创建MCP Server
	server := mcp.NewMCPServer(connector)

	// 启动Server
	if err := server.Start(); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to start MCP Server: %v", err)
		os.Exit(1)
	}
}
