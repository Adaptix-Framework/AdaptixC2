//go:build !daemon || windows

package daemon

func Daemonize() bool {
	return false
}
