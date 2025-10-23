package mcp

import (
	"fmt"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// registerPrompts 注册所有Prompts
func (s *MCPServer) registerPrompts() {
	s.prompts["reconnaissance"] = s.handleReconnaissancePrompt
	s.prompts["lateral_movement"] = s.handleLateralMovementPrompt
	s.prompts["privilege_escalation"] = s.handlePrivilegeEscalationPrompt

	utils.InfoLogger.Println("💬 Registered Prompts: reconnaissance, lateral_movement, privilege_escalation")
}

// handleReconnaissancePrompt 侦察提示词
func (s *MCPServer) handleReconnaissancePrompt(params map[string]interface{}) (interface{}, error) {
	target, ok := params["target"].(string)
	if !ok {
		target = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。当前目标：%s

请执行以下侦察步骤：
1. 使用现有Agent扫描目标网段
2. 识别开放端口和服务
3. 收集主机信息
4. 汇总侦察结果

可用资源：
- agents://list - 查看所有可用Agent
- agents://{id}/console - 查看Agent控制台输出

可用工具：
- execute_command - 在Agent上执行命令
  参数: agent_id (string), command (string)

示例命令：
- whoami - 查看当前用户
- ipconfig - 查看网络配置
- netstat - 查看网络连接
- arp - 查看ARP表
- smartscan - 扫描网段`, target)

	return GetPromptResult{
		Description: "Execute reconnaissance on a target",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}

// handleLateralMovementPrompt 横向移动提示词
func (s *MCPServer) handleLateralMovementPrompt(params map[string]interface{}) (interface{}, error) {
	fromAgent, ok := params["from_agent"].(string)
	if !ok {
		fromAgent = "unknown"
	}

	targetHost, ok := params["target_host"].(string)
	if !ok {
		targetHost = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。执行横向移动：

源Agent: %s
目标主机: %s

步骤：
1. 在源Agent上侦察目标主机
2. 创建合适的Listener
3. 生成适配的Agent payload
4. 传输并执行Agent

可用资源：
- agents://list - 查看所有Agent
- listeners://list - 查看所有Listener
- extenders://listeners - 查看可用的Listener类型

可用工具：
- execute_command - 执行命令
- create_listener - 创建Listener
  参数: name (string), type (string), config (object)
- stop_listener - 停止Listener
  参数: name (string)

示例：
1. 创建HTTP Listener:
   create_listener(
     name="http_8080",
     type="beacon",
     config={"port": 8080, "protocol": "http"}
   )
2. 在源Agent上执行侦察:
   execute_command(agent_id="%s", command="smartscan %s")`, fromAgent, targetHost, fromAgent, targetHost)

	return GetPromptResult{
		Description: "Execute lateral movement to a new host",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}

// handlePrivilegeEscalationPrompt 提权提示词
func (s *MCPServer) handlePrivilegeEscalationPrompt(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		agentID = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。执行权限提升：

目标Agent: %s

步骤：
1. 检查当前权限
2. 识别可用的提权方法
3. 执行提权
4. 验证新权限

可用资源：
- agents://%s - 查看Agent详情
- agents://%s/console - 查看控制台输出

可用工具：
- execute_command - 执行命令

常用提权命令：
- whoami - 查看当前用户和权限
- privcheck tokenpriv - 检查Token权限
- privcheck hijackablepath - 检查可劫持路径
- privcheck unquotedsvc - 检查未引用的服务路径
- privcheck vulndrivers - 检查易受攻击的驱动
- getsystem token - 尝试提升到SYSTEM
- potato-dcom - 使用DCOMPotato提权`, agentID, agentID, agentID)

	return GetPromptResult{
		Description: "Execute privilege escalation on an agent",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}
