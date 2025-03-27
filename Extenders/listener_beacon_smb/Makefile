all: clean
	@ echo "    * Building listener_beacon_smb plugin"
	@ mkdir dist
	@ cp _ui_listener.json ./dist/
	@ go build -buildmode=plugin -ldflags="-s -w" -o ./dist/listener_beacon_smb.so pl_main.go pl_smb.go pl_listener.go
	@ echo "      done..."

clean:
	@ rm -rf dist
