//go:build !windows

package coffer

import (
	"fmt"
	"gopher/utils"
)

// Dummy function for Linux and MacOS
func Load(coffBytes []byte, argBytes []byte) ([]utils.BofMsg, error) {
	return []utils.BofMsg{}, fmt.Errorf("Need Windows!")
}
