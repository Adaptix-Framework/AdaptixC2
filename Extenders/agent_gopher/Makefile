all: clean
	@ echo "    * Building agent_gopher plugin"
	@ mkdir dist
	@ cp config.json ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/agent_gopher.so pl_main.go pl_agent.go pl_utils.go
	@ echo "      done..."

	@ echo "    * Building agent"
	@ (cd src_beacon && $(MAKE) --no-print-directory)
	@ mv src_beacon/objects_http ./dist/objects_http
	@ mv src_beacon/objects_smb ./dist/objects_smb
	@ echo "      done..."

clean:
	@ rm -rf dist
