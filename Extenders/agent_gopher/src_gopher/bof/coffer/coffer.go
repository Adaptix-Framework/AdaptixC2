//go:build !windows

package coffer

import "fmt"

// Dummy function for Linux and MacOS
func Load(coffBytes []byte, argBytes []byte) (string, error) {
	return "", fmt.Errorf("Need Windows!")
}
