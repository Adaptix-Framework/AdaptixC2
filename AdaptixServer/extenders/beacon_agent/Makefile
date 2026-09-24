include $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../../../go_ldflags.mk

all: clean
	@ echo "    * Building agent_beacon plugin"
	@ mkdir dist
	@ cp config.yaml ax_config.axs ./dist/
	@ go build -buildmode=plugin -ldflags="$(GO_LDFLAGS)" -o ./dist/agent_beacon.so pl_main.go pl_packer.go pl_utils.go pl_sideloading.go
	@ echo "      done..."

	@ echo "    * Building agent"
	@ (cd src_beacon && $(MAKE) --no-print-directory)
	@ mv src_beacon/objects_http ./dist/objects_http
	@ mv src_beacon/objects_smb ./dist/objects_smb
	@ mv src_beacon/objects_tcp ./dist/objects_tcp
	@ mv src_beacon/objects_dns ./dist/objects_dns
	@ echo "      done..."

clean:
	@ rm -rf dist
