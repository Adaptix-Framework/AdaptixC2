all: clean
	@ echo "    * Building listener_beacon_http plugin"
	@ mkdir dist
	@ cp config.json ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/listener_beacon_http.so pl_main.go pl_http.go pl_listener.go
	@ echo "      done..."

clean:
	@ rm -rf dist
