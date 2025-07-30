all: clean
	@ echo "    * Building agent_beacon plugin"
	@ mkdir dist
	@ cp config.json ax_config.axs ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/agent_beacon.so pl_main.go pl_agent.go pl_packer.go pl_utils.go
	@ echo "      done..."

	@ echo "    * Building agent"
	@ (cd src_beacon && $(MAKE) --no-print-directory)
	@ mv src_beacon/objects_http ./dist/objects_http
	@ mv src_beacon/objects_smb ./dist/objects_smb
	@ mv src_beacon/objects_tcp ./dist/objects_tcp
	@ echo "      done..."

clean:
	@ rm -rf dist
