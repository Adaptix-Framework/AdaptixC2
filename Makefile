all: clean prepare server client extenders

DIST_DIR := dist

# 检测操作系统
UNAME_S := $(shell uname -s)

prepare:
	@if [ ! -d "$(DIST_DIR)" ]; then \
		mkdir "$(DIST_DIR)"; \
		echo "[+] Created '$(DIST_DIR)' directory"; \
	fi

clean:
	@ rm -rf $(DIST_DIR)

server: prepare
	@ echo "[*] Building adaptixserver..."
	@ cd AdaptixServer && go build -buildvcs=false -ldflags="-s -w" -o adaptixserver > /dev/null 2>build_error.log || { echo "[ERROR] Failed to build AdaptixServer:"; cat build_error.log >&2; exit 1; }
ifeq ($(UNAME_S),Linux)
	@ if command -v setcap >/dev/null 2>&1; then \
		sudo setcap 'cap_net_bind_service=+ep' AdaptixServer/adaptixserver; \
	else \
		echo "[WARNING] setcap not found, skipping capability setting"; \
	fi
else
	@ echo "[INFO] Skipping setcap on $(UNAME_S)"
endif
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

# 添加便捷的清理命令
clean-all: clean
	@ echo "[*] Cleaning all build artifacts..."
	@ find . -name "*.o" -delete
	@ find . -name "*.so" -delete
	@ find . -name "*.a" -delete
	@ find . -name "build_error.log" -delete
	@ find . -name "cmake_error.log" -delete
	@ echo "[+] All artifacts cleaned"

# 添加帮助信息
help:
	@ echo "AdaptixC2 Build System"
	@ echo ""
	@ echo "Available targets:"
	@ echo "  all        - Build everything (server, client, extenders)"
	@ echo "  server     - Build only the server"
	@ echo "  client     - Build only the client"
	@ echo "  extenders  - Build only the extenders"
	@ echo "  clean      - Remove dist directory"
	@ echo "  clean-all  - Remove all build artifacts"
	@ echo "  help       - Show this help message"
	@ echo ""
	@ echo "Platform: $(UNAME_S)"

.PHONY: all server client extenders clean clean-all help prepare
