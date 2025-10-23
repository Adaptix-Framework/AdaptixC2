package utils

import "fmt"

// MCPError MCP错误类型
type MCPError struct {
	Code    int
	Message string
	Data    interface{}
}

func (e *MCPError) Error() string {
	return fmt.Sprintf("MCP Error %d: %s", e.Code, e.Message)
}

// 常见错误码
const (
	ErrCodeInvalidRequest   = -32600
	ErrCodeMethodNotFound   = -32601
	ErrCodeInvalidParams    = -32602
	ErrCodeInternalError    = -32603
	ErrCodeParseError       = -32700
	ErrCodeResourceNotFound = -32001
	ErrCodeToolNotFound     = -32002
	ErrCodeClientError      = -32003
)

func NewInvalidRequest(msg string) *MCPError {
	return &MCPError{Code: ErrCodeInvalidRequest, Message: msg}
}

func NewMethodNotFound(method string) *MCPError {
	return &MCPError{Code: ErrCodeMethodNotFound, Message: fmt.Sprintf("Method not found: %s", method)}
}

func NewInvalidParams(msg string) *MCPError {
	return &MCPError{Code: ErrCodeInvalidParams, Message: msg}
}

func NewInternalError(msg string) *MCPError {
	return &MCPError{Code: ErrCodeInternalError, Message: msg}
}

func NewClientError(msg string) *MCPError {
	return &MCPError{Code: ErrCodeClientError, Message: msg}
}
