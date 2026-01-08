all: clean
	@ echo "    * Building beacon_listener_dns plugin"
	@ mkdir dist
	@ cp config.json ax_config.axs ./dist/
	@ GOEXPERIMENT=jsonv2,greenteagc go build -buildmode=plugin -ldflags="-s -w" -o ./dist/beacon_listener_dns.so pl_main.go pl_transport.go
	@ echo "      done..."

clean:
	@ rm -rf dist