#!/usr/bin/env bash

set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-fedora.sh <client|server|extenders|server-ext|all>

Build AdaptixC2 on Fedora with Podman (preferred) or Docker. The build runs
inside the upstream Debian/Ubuntu stages, so no host Qt or Go packages are
required.

Environment:
  CONTAINER_ENGINE=podman|docker   Override automatic engine selection.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_target="${1:-}"

case "$build_target" in
    client|server|extenders|server-ext|all) ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

engine="${CONTAINER_ENGINE:-}"
if [[ -z "$engine" ]]; then
    if command -v podman >/dev/null 2>&1; then
        engine=podman
    elif command -v docker >/dev/null 2>&1; then
        engine=docker
    else
        fail "install Podman with: sudo dnf install podman"
    fi
fi

command -v "$engine" >/dev/null 2>&1 || fail "$engine is not installed"

run_copy_container() {
    local image="$1"
    local output_dir="$2"
    local copy_command="$3"
    local -a run_args=(run --rm)

    mkdir -p "$output_dir"

    if [[ "$engine" == podman ]]; then
        run_args+=(--userns=keep-id)
    else
        run_args+=(--user "$(id -u):$(id -g)")
    fi

    run_args+=(-v "${output_dir}:/output:Z" "$image" sh -c "$copy_command")
    "$engine" "${run_args[@]}"
}

build_client() {
    local image="localhost/adaptix-client-builder:fedora"
    local output_dir="$repo_root/AdaptixClient/client-dist"

    printf '[*] Building the Adaptix client AppImage with %s...\n' "$engine"
    "$engine" build --network=host --target build-client -t "$image" "$repo_root"
    run_copy_container "$image" "$output_dir" 'cp -a /client-dist/. /output/'
    printf '[+] Client artifacts: %s\n' "$output_dir"
}

build_server() {
    local image="localhost/adaptix-server-builder:fedora"
    local output_dir="$repo_root/AdaptixServer/server-dist"

    printf '[*] Building the Adaptix server with %s...\n' "$engine"
    "$engine" build --target build-server -t "$image" "$repo_root"
    run_copy_container "$image" "$output_dir" \
        'cp -a /app/dist/adaptixserver /app/dist/ssl_gen.sh /app/dist/profile.yaml /app/dist/404page.html /output/'
    printf '[+] Server artifacts: %s\n' "$output_dir"
}

build_extenders() {
    local image="localhost/adaptix-extenders-builder:fedora"
    local output_dir="$repo_root/AdaptixServer/server-dist/extenders"

    printf '[*] Building the Adaptix extenders with %s...\n' "$engine"
    "$engine" build --target build-extenders -t "$image" "$repo_root"
    run_copy_container "$image" "$output_dir" 'cp -a /app/dist/extenders/. /output/'
    printf '[+] Extender artifacts: %s\n' "$output_dir"
}

build_server_ext() {
    local image="localhost/adaptix-server-ext-builder:fedora"
    local output_dir="$repo_root/AdaptixServer/server-dist"

    printf '[*] Building the Adaptix server and extenders with %s...\n' "$engine"
    "$engine" build --target build-server-ext -t "$image" "$repo_root"
    run_copy_container "$image" "$output_dir" 'cp -a /app/dist/. /output/'
    printf '[+] Server and extender artifacts: %s\n' "$output_dir"
}

case "$build_target" in
    client) build_client ;;
    server) build_server ;;
    extenders) build_extenders ;;
    server-ext) build_server_ext ;;
    all)
        build_server_ext
        build_client
        ;;
esac
