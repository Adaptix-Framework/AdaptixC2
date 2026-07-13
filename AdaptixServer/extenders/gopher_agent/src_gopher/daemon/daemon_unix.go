//go:build daemon && !windows

package daemon

import "github.com/sevlyar/go-daemon"

func Daemonize() bool {
	cntxt := &daemon.Context{}

	d, err := cntxt.Reborn()
	if err != nil {
		panic(err)
	}

	if d != nil {
		return true
	}

	go func() {
		defer cntxt.Release()
	}()

	return false
}
