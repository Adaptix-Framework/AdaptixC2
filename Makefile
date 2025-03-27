all: clean prepare server client extenders

DIST_DIR := dist

prepare:
	@if [ ! -d "$(DIST_DIR)" ]; then \
		mkdir "$(DIST_DIR)"; \
		echo "[+] Created '$(DIST_DIR)' directory"; \
	fi

clean:
	@ rm -rf $(DIST_DIR)

server: prepare
	@ echo "[*] Building adaptixserver..."
	@ cd AdaptixServer && go build -ldflags="-s -w" -o adaptixserver > /dev/null 2>build_error.log || { echo "[ERROR] Failed to build AdaptixServer:"; cat build_error.log >&2; exit 1; }     # for static build use CGO_ENABLED=0
	@ sudo setcap 'cap_net_bind_service=+ep' AdaptixServer/adaptixserver
	@ mv AdaptixServer/adaptixserver ./$(DIST_DIR)/
	@ cp AdaptixServer/ssl_gen.sh AdaptixServer/profile.json AdaptixServer/404page.html ./$(DIST_DIR)/
	@ echo "[+] done"

client: prepare
	@ echo "[*] Building AdaptixClient..."
	@ cd AdaptixClient && cmake . > /dev/null 2>cmake_error.log || { echo "[ERROR] CMake failed:"; cat cmake_error.log >&2; exit 1; }
	@ cd AdaptixClient && make --no-print-directory
	@ mv ./AdaptixClient/AdaptixClient ./$(DIST_DIR)/
	@ echo "[+] done"

### Extenders here

EXTENDER_DIRS := $(shell find Extenders -maxdepth 1 -type d -not -path "." -exec test -f {}/Makefile \; -print)

extenders: prepare
	@ echo "[*] Building default extenders"
	@ mkdir -p $(DIST_DIR)/extenders
	@ for dir in $(EXTENDER_DIRS); do \
		(cd $$dir && $(MAKE) --no-print-directory); \
		plugin_name=$$(basename $$dir); \
		mv $$dir/dist $(DIST_DIR)/extenders/$$plugin_name; \
	done
	@ echo "[+] done"
