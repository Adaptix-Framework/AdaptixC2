//go:build !debug

package utils

// DebugLog is a no-op in release builds. The compiler eliminates these calls.
func DebugLog(format string, args ...interface{}) {}
