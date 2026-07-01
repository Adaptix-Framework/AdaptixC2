# AGENTS.md

## Architecture

- **`AdaptixServer/`** — Go server (module `AdaptixServer`). Entrypoint: `main.go`.
- **`AdaptixClient/`** — C++/Qt6 GUI client. Built via CMake (`CMakeLists.txt`).
- **`AdaptixServer/extenders/`** — 7 Go plugin modules (agents + listeners). Each has its own `go.mod` + `Makefile`.
- **`AdaptixCLI/`** — Go CLI tool for server interaction (independent module). Designed for AI/script automation.
- **`AdaptixServer/core/`** — Server internals: `connector/`, `server/`, `database/`, `extender/`, `profile/`, `eventing/`, `axscript/`, `utils/`.
- Go workspace file (`go.work`) ties the server module + all 7 extender modules together.

## Build

All builds use the top-level `Makefile`:

| Command | What it builds |
|---------|---------------|
| `make all` | Server + client + extenders |
| `make server` | Go server only (requires `ssl_gen.sh`, `profile.yaml`, `404page.html` alongside binary) |
| `make server-ext` | Server + extenders (ideal for VPS, no client) |
| `make extenders` | All extender plugins only |
| `make client` | C++/Qt6 client (single-threaded) |
| `make client-fast` | C++/Qt6 client (parallel build) |
| `make cli` | `AdaptixCLI/` CLI tool only |
| `make install-cli` | Build CLI and copy to `/usr/local/bin` |

**Important Go build flags**: The server and extenders require `GOEXPERIMENT=jsonv2,greenteagc`. The Makefile handles this — don't omit it if building manually.

**Extenders are Go plugins**: Built with `-buildmode=plugin`, output as `.so` files. Their Makefiles also cross-compile the actual agent binaries (e.g., `objects_http/`, `objects_smb/`) via sub-make.

**Client prerequisites**: Qt6, OpenSSL, CMake ≥ 3.28. On macOS with Homebrew: `brew install qt@6 cmake openssl`. On Linux see `pre_install_linux_all.sh`.

**Windows client build**: Use `AdaptixClient/build.bat` (requires MSYS2/MinGW).

## Run

```
./dist/adaptixserver -profile profile.yaml [-debug]
```

- Server listens on the interface/port defined in `profile.yaml` (default: `0.0.0.0:4321`).
- TLS certs are generated via `ssl_gen.sh` before first run.
- Client connects via the GUI dialog.

## Docker

- Build: `make docker-build-server`, `make docker-build-extenders`, `make docker-build-server-ext`
- Runtime: `make docker-up` / `make docker-down` / `make docker-logs`
- Profiles: `build-server`, `build-extenders`, `build-server-ext`, `build-client`, `runtime`

## Repo conventions

- **Active dev branch**: `dev-v1.3`. All PRs go to the `dev` branch (see README CONTRIBUTING).
- **No tests exist** — there are no `_test.go` files, no test runner config, no CI pipelines.
- **No linter/formatter config** — no `.golangci.yml`, `.clang-format`, or pre-commit hooks.
- Go version: **1.26.1** (`go.mod`, `go.work`). The Docker runtime installs `go1.26.4`.
- Server module name is `AdaptixServer` (capital A, no dot). Extender module names use underscore convention (e.g., `adaptix_agent_beacon`).

## Key files

| File | Purpose |
|------|---------|
| `AdaptixServer/profile.yaml` | Server config (listeners, extenders, TLS, auth) |
| `AdaptixServer/ssl_gen.sh` | Generate self-signed RSA certs |
| `AdaptixServer/404page.html` | Default HTTP 404 error page |
| `AdaptixServer/go.work` | Multi-module Go workspace |
| `AdaptixClient/CMakeLists.txt` | Client build definition, lists all source files |
| `AdaptixCLI/go.mod` | CLI tool module (standalone, not in go.work) |
| `Makefile` | Top-level build orchestrator |
