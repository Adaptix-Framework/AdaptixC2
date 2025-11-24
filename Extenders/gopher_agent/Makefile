all: clean
	@ echo "    * Building agent_gopher plugin"
	@ mkdir dist
	@ cp config.json ax_config.axs ./dist/
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="-s -w" -o ./dist/agent_gopher.so pl_main.go pl_agent.go pl_utils.go
	@ echo "      done..."

	@ echo "    * Building agent"
	@ (cd src_gopher && $(MAKE) --no-print-directory)
	@ cp -r src_gopher ./dist/src_gopher
	@ echo "      done..."

clean:
	@ rm -rf dist
