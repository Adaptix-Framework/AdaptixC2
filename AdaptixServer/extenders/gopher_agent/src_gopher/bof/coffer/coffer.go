//go:build !windows

package coffer

import (
	"fmt"
	"gopher/utils"
)

type AsyncBof struct {
	Output chan interface{}
	Done   chan struct{}
}

func (a *AsyncBof) Stop()    {}
func (a *AsyncBof) Cleanup() {}

func Load(coffBytes []byte, argBytes []byte) ([]utils.BofMsg, error) {
	return []utils.BofMsg{}, fmt.Errorf("Need Windows!")
}

func LoadAsync(coffBytes []byte, argBytes []byte, wakeupFunc func()) (*AsyncBof, error) {
	return nil, fmt.Errorf("Need Windows!")
}
