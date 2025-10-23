package utils

import (
	"log"
	"os"
)

var (
	InfoLogger  *log.Logger
	WarnLogger  *log.Logger
	ErrorLogger *log.Logger
	DebugLogger *log.Logger
)

func init() {
	// 所有日志输出到stderr，保持stdout干净用于JSON-RPC通信
	InfoLogger = log.New(os.Stderr, "[INFO] ", log.Ldate|log.Ltime|log.Lshortfile)
	WarnLogger = log.New(os.Stderr, "[WARN] ", log.Ldate|log.Ltime|log.Lshortfile)
	ErrorLogger = log.New(os.Stderr, "[ERROR] ", log.Ldate|log.Ltime|log.Lshortfile)
	DebugLogger = log.New(os.Stderr, "[DEBUG] ", log.Ldate|log.Ltime|log.Lshortfile)
}
