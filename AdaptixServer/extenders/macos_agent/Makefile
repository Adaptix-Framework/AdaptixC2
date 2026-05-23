all: clean
	@ echo "    * Building agent_macos plugin"
	@ mkdir dist
	@ cp config.yaml ax_config.axs ./dist/
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="-s -w" -o ./dist/agent_macos.so pl_main.go pl_utils.go pl_hashes_macos.go pl_encoder_macos.go
	@ echo "      done..."

	@ echo "    * Preparing macOS agent sources"
	@ cp -r src_macos ./dist/src_macos
	@ cp -r src_agent ./dist/src_agent
	@ echo "      done..."

clean:
	@ rm -rf dist
