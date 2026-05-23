//go:build debug

package utils

import (
	"fmt"
	"os"
	"time"
)

// DebugLog prints debug messages to stderr when built with -tags=debug.
// Never included in production payloads.
func DebugLog(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	fmt.Fprintf(os.Stderr, "[DBG %s] %s\n", time.Now().Format("15:04:05"), msg)
}
