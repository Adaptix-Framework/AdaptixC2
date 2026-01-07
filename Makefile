all: clean prepare server client extenders

DIST_DIR := dist

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
  NPROC := $(shell nproc)
endif

ifeq ($(UNAME_S),Darwin)
  NPROC := $(shell sysctl -n hw.ncpu)
endif



### CLEAN ###

### DEPS ###
deps:
	@if [ ! -d "AdaptixClient/Libs/donut" ]; then \
		echo "[*] Cloning donut library..."; \
		git clone https://github.com/TheWover/donut AdaptixClient/Libs/donut; \
	fi

prepare:
	@if [ ! -d "$(DIST_DIR)" ]; then \
		mkdir "$(DIST_DIR)"; \
		echo "[+] Created '$(DIST_DIR)' directory"; \
	fi

clean:
	@ rm -rf $(DIST_DIR)

clean-all: clean
	@ echo "[*] Cleaning all build artifacts..."
	@ find . -name "*.o" -delete
	@ find . -name "*.so" -delete
	@ find . -name "*.a" -delete
	@ find . -name "build_error.log" -delete
	@ find . -name "cmake_error.log" -delete
	@ echo "[+] All artifacts cleaned"

docker-clean:
	@ echo "[*] Cleaning Docker containers and images..."
	@ docker compose --profile build-server --profile build-extenders --profile build-server-ext down --rmi local 2>/dev/null || true
	@ echo "[+] Docker containers and images cleaned"

docker-clean-all: docker-clean
	@ echo "[*] Cleaning all Docker artifacts (containers, images, volumes, networks)..."
	@ docker compose --profile build-server --profile build-extenders --profile build-server-ext --profile runtime down --rmi all --volumes 2>/dev/null || true
	@ echo "[*] Cleaning build output directories..."
	@ rm -rf AdaptixServer/server-dist 2>/dev/null || true
	@ echo "[+] All Docker artifacts cleaned"



### EXTENDERS ###

EXTENDER_DIRS := $(shell find AdaptixServer/extenders -maxdepth 1 -type d -not -path "$(DIST_DIR)" -exec test -f {}/Makefile \; -print)

extenders: prepare
	@ echo "[*] Building default extenders"
	@ mkdir -p $(DIST_DIR)/extenders
	@ for dir in $(EXTENDER_DIRS); do \
		(cd $$dir && $(MAKE) --no-print-directory); \
		plugin_name=$$(basename $$dir); \
		mv $$dir/dist $(DIST_DIR)/extenders/$$plugin_name; \
	done
	@ echo "[+] done"


### SERVER ###

server: prepare
	@ echo "[*] Building adaptixserver..."
	@ cd AdaptixServer && GOEXPERIMENT=jsonv2,greenteagc go build -buildvcs=false -ldflags="-s -w" -o adaptixserver > /dev/null 2>build_error.log || { echo "[ERROR] Failed to build AdaptixServer:"; cat build_error.log >&2; exit 1; }     # for static build use CGO_ENABLED=0
	@ mv AdaptixServer/adaptixserver ./$(DIST_DIR)/
	@ cp AdaptixServer/ssl_gen.sh AdaptixServer/profile.json AdaptixServer/404page.html ./$(DIST_DIR)/
	@ echo "[+] done"

server-ext: clean server extenders



### CLIENT ###

client: prepare deps
	@ echo "[*] Building AdaptixClient..."
	@ cd AdaptixClient && cmake . > /dev/null 2>cmake_error.log || { echo "[ERROR] CMake failed:"; cat cmake_error.log >&2; exit 1; }
	@ cd AdaptixClient && make --no-print-directory
	@ mv ./AdaptixClient/AdaptixClient ./$(DIST_DIR)/
	@ echo "[+] done"

client-fast: prepare deps
	@ echo "[*] Building AdaptixClient in $(NPROC) threads..."
	@ cd AdaptixClient && cmake . > /dev/null 2>cmake_error.log || { echo "[ERROR] CMake failed:"; cat cmake_error.log >&2; exit 1; }
	@ cd AdaptixClient && make --no-print-directory -j$(NPROC)
	@ mv ./AdaptixClient/AdaptixClient ./$(DIST_DIR)/
	@ echo "[+] done"



### DOCKER BUILD ###

docker-build-server:
	@ echo "[*] Building server via Docker..."
	@ docker compose --profile build-server build
	@ docker compose --profile build-server up --abort-on-container-exit
	@ docker compose --profile build-server down
	@ echo "[+] Server built via Docker"

docker-build-extenders:
	@ echo "[*] Building extenders via Docker..."
	@ docker compose --profile build-extenders build
	@ docker compose --profile build-extenders up --abort-on-container-exit
	@ docker compose --profile build-extenders down
	@ echo "[+] Extenders built via Docker"

docker-build-server-ext:
	@ echo "[*] Building server and extenders via Docker..."
	@ docker compose --profile build-server-ext build
	@ docker compose --profile build-server-ext up --abort-on-container-exit
	@ docker compose --profile build-server-ext down
	@ echo "[+] Server and extenders built via Docker"

docker-build-all: docker-build-server-ext
	@ echo "[+] All Docker builds completed"



### DOCKER RUNTIME ###

docker-up:
	@ echo "[*] Starting Adaptix server runtime container..."
	@ docker compose --profile runtime up -d
	@ echo "[+] Runtime container started"
	@ echo "[*] Use 'make docker-logs' to view logs"
	@ echo "[*] Use 'make docker-down' to stop container"

docker-down:
	@ echo "[*] Stopping Adaptix server runtime container..."
	@ docker compose --profile runtime down
	@ echo "[+] Runtime container stopped"

docker-logs:
	@ docker compose --profile runtime logs -f

docker-restart: docker-down docker-up



### HELP ###

help:
	@ echo "AdaptixC2 Build System"
	@ echo ""
	@ echo "Available targets:"
	@ echo "  all              - Build everything (server, client, extenders)"
	@ echo "  extenders        - Build only the extenders"
	@ echo "  server-ext       - Build server and extenders (no client, ideal for VPS)"
	@ echo "  server           - Build only the server"
	@ echo "  client-fast      - Build only the client in multithread mode (fast build)"
	@ echo "  client           - Build only the client"
	@ echo "  clean            - Remove dist directory"
	@ echo "  clean-all        - Remove all build artifacts"
	@ echo ""
	@ echo "Docker commands:"
	@ echo "  docker-build-server     - Build server via Docker Compose"
	@ echo "  docker-build-extenders  - Build extenders via Docker Compose"
	@ echo "  docker-build-server-ext - Build server and extenders via Docker Compose"
	@ echo "  docker-build-all        - Build server and extenders via Docker Compose (alias)"
	@ echo "  docker-up               - Start runtime container (detached)"
	@ echo "  docker-down             - Stop runtime container"
	@ echo "  docker-logs             - View runtime container logs (follow mode)"
	@ echo "  docker-restart          - Restart runtime container"
	@ echo "  docker-clean            - Remove Docker containers and images (builders only)"
	@ echo "  docker-clean-all        - Remove all Docker artifacts (containers, images, volumes, networks)"
	@ echo ""
	@ echo "  help             - Show this help message"
	@ echo ""
	@ echo "Platform: $(UNAME_S) [$(NPROC) proc]"

.PHONY: all extenders server-ext server client clean clean-all docker-build-server docker-build-extenders docker-build-server-ext docker-build-all docker-up docker-down docker-logs docker-restart docker-clean docker-clean-all help prepare
