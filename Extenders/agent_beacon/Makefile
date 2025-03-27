all: clean
	@ echo "    * Building agent_beacon plugin"
	@ mkdir dist
	@ cp _cmd_agent.json _ui_agent.json ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/agent_beacon.so pl_main.go pl_agent.go pl_packer.go pl_utils.go
	@ echo "      done..."

	@ echo "    * Building agent"
	@ (cd src_beacon && $(MAKE) --no-print-directory)
	@ mv src_beacon/objects_http ./dist/objects_http
	@ mv src_beacon/objects_smb ./dist/objects_smb
	@ echo "      done..."

clean:
	@ rm -rf dist
