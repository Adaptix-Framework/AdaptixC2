include $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../../../go_ldflags.mk

all: clean
	@ echo "    * Building beacon_listener_dns plugin"
	@ mkdir dist
	@ cp config.yaml ax_config.axs ./dist/
	@ go build -buildmode=plugin -ldflags="$(GO_LDFLAGS)" -o ./dist/beacon_listener_dns.so pl_main.go pl_transport.go
	@ echo "      done..."

clean:
	@ rm -rf dist