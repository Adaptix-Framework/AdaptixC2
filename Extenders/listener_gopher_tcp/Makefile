all: clean
	@ echo "    * Building listener_gopher_tcp plugin"
	@ mkdir dist
	@ cp config.json ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/listener_gopher_tcp.so pl_main.go pl_listener.go pl_tcp.go map.go
	@ echo "      done..."

clean:
	@ rm -rf dist
