package mcp

import (
	"encoding/json"
	"fmt"
	"strings"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// registerResources 注册所有Resources
func (s *MCPServer) registerResources() {
	s.resources["agents"] = s.handleAgentsResource
	s.resources["listeners"] = s.handleListenersResource
	s.resources["credentials"] = s.handleCredentialsResource
	s.resources["tasks"] = s.handleTasksResource
	s.resources["extenders"] = s.handleExtendersResource

	utils.InfoLogger.Println("📚 Registered Resources: agents, listeners, credentials, tasks, extenders")
}

// routeResource 路由Resource请求
func (s *MCPServer) routeResource(uri string) (ResourceContents, error) {
	// 解析URI: scheme://path
	parts := strings.SplitN(uri, "://", 2)
	if len(parts) != 2 {
		return ResourceContents{}, fmt.Errorf("invalid URI format: %s", uri)
	}

	scheme := parts[0]

	handler, ok := s.resources[scheme]
	if !ok {
		return ResourceContents{}, fmt.Errorf("unknown resource scheme: %s", scheme)
	}

	// 调用Handler
	data, err := handler(uri)
	if err != nil {
		return ResourceContents{}, err
	}

	// 将数据转换为JSON
	jsonData, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return ResourceContents{}, err
	}

	return ResourceContents{
		URI:      uri,
		MimeType: "application/json",
		Text:     string(jsonData),
	}, nil
}

// handleAgentsResource 处理agents://资源
func (s *MCPServer) handleAgentsResource(uri string) (interface{}, error) {
	path := strings.TrimPrefix(uri, "agents://")

	// agents://list - 获取所有Agent列表
	if path == "list" {
		resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
			"command": "list_agents",
		})
		if err != nil {
			return nil, fmt.Errorf("failed to get agents list: %w", err)
		}

		return resp.Data, nil
	}

	// agents://{id}/console - 获取Agent控制台输出
	if strings.Contains(path, "/console") {
		agentID := strings.TrimSuffix(path, "/console")

		resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
			"command":  "get_agent_console",
			"agent_id": agentID,
		})
		if err != nil {
			return nil, fmt.Errorf("failed to get agent console: %w", err)
		}

		return resp.Data, nil
	}

	// agents://{id} - 获取特定Agent详情
	agentID := path

	// 先获取列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "list_agents",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get agents list: %w", err)
	}

	// 查找对应的Agent
	agents, ok := resp.Data["agents"].([]interface{})
	if !ok {
		return nil, fmt.Errorf("invalid agents data format")
	}

	for _, a := range agents {
		agent, ok := a.(map[string]interface{})
		if !ok {
			continue
		}

		if agent["id"] == agentID {
			return agent, nil
		}
	}

	return nil, fmt.Errorf("agent not found: %s", agentID)
}

// handleListenersResource 处理listeners://资源
func (s *MCPServer) handleListenersResource(uri string) (interface{}, error) {
	path := strings.TrimPrefix(uri, "listeners://")

	// listeners://list - 获取所有Listener列表
	if path == "list" {
		resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
			"command": "list",
		})
		if err != nil {
			return nil, fmt.Errorf("failed to get listeners list: %w", err)
		}

		return resp.Data, nil
	}

	// listeners://{name} - 获取特定Listener详情
	listenerName := path

	resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
		"command": "get_info",
		"name":    listenerName,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get listener info: %w", err)
	}

	return resp.Data, nil
}

// handleCredentialsResource 处理credentials://资源
func (s *MCPServer) handleCredentialsResource(uri string) (interface{}, error) {
	// TODO: 实现CredentialsHandler后启用
	return map[string]interface{}{
		"message": "Credentials resource not yet implemented",
		"note":    "Requires CredentialsHandler in Client",
	}, nil
}

// handleTasksResource 处理tasks://资源
func (s *MCPServer) handleTasksResource(uri string) (interface{}, error) {
	// TODO: 实现HistoryHandler后启用
	return map[string]interface{}{
		"message": "Tasks resource not yet implemented",
		"note":    "Requires HistoryHandler in Client",
	}, nil
}

// handleExtendersResource 处理extenders://资源
func (s *MCPServer) handleExtendersResource(uri string) (interface{}, error) {
	path := strings.TrimPrefix(uri, "extenders://")

	// extenders://listeners - 获取可用的Listener类型
	if path == "listeners" {
		resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
			"command": "list_extenders",
		})
		if err != nil {
			return nil, fmt.Errorf("failed to get listener extenders: %w", err)
		}

		return resp.Data, nil
	}

	// extenders://agents - 获取可用的Agent类型
	if path == "agents" {
		// TODO: 实现AgentGeneratorHandler后启用
		return map[string]interface{}{
			"message": "Agent extenders not yet implemented",
			"note":    "Requires AgentGeneratorHandler in Client",
		}, nil
	}

	return nil, fmt.Errorf("unknown extenders path: %s", path)
}
