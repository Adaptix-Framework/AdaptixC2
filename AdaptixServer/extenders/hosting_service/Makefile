all: clean
	@ echo "    * Building hosting service plugin"
	@ mkdir dist
	@ cp config.yaml ax_config.axs ./dist/
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="-s -w" -o ./dist/service_hosting.so pl_main.go
	@ echo "      done..."

clean:
	@ rm -rf dist
