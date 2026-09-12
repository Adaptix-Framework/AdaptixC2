include $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../../../go_ldflags.mk

all: clean
	@ echo "    * Building listener_beacon_tcp plugin"
	@ mkdir dist
	@ cp config.yaml ax_config.axs ./dist/
	@ go build -buildmode=plugin -ldflags="$(GO_LDFLAGS)" -o ./dist/listener_beacon_tcp.so pl_main.go pl_transport.go
	@ echo "      done..."

clean:
	@ rm -rf dist
