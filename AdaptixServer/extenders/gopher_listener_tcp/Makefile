all: clean
	@ echo "    * Building listener_gopher_tcp plugin"
	@ mkdir dist
	@ cp config.json ax_config.axs ./dist/
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="-s -w" -o ./dist/listener_gopher_tcp.so pl_main.go pl_transport.go map.go
	@ echo "      done..."

clean:
	@ rm -rf dist
