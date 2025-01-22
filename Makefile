all: clean prepare server client extenders

prepare:
	@ mkdir dist
	@ echo "[+] Created 'dist' directory"

server:
	@ echo "[*] Building adaptixserver..."
	@ cd AdaptixServer && go build -ldflags="-s -w" -o adaptixserver > /dev/null 2>&1       # for static build use CGO_ENABLED=0
	@ sudo setcap 'cap_net_bind_service=+ep' AdaptixServer/adaptixserver
	@ mv AdaptixServer/adaptixserver ./dist/
	@ cp AdaptixServer/ssl_gen.sh AdaptixServer/profile.json ./dist/
	@ echo "[+] done"

client:
	@ echo "[*] Building AdaptixClient..."
	@ cd AdaptixClient && cmake . > /dev/null 2>&1
	@ cd AdaptixClient && make --no-print-directory
	@ mv ./AdaptixClient/AdaptixClient ./dist/
	@ echo "[+] done"

### Extenders here

EXTENDER_DIRS := $(shell find Extenders -maxdepth 1 -type d -not -path "." -exec test -f {}/Makefile \; -print)

extenders:
	@ echo "[*] Building default extenders"
	@ mkdir -p dist/extenders
	@for dir in $(EXTENDER_DIRS); do \
		(cd $$dir && $(MAKE) --no-print-directory); \
		plugin_name=$$(basename $$dir); \
		mv $$dir/dist dist/extenders/$$plugin_name; \
	done
	@ echo "[+] done"

clean:
	@ rm -rf dist
