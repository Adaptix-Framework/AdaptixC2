#!/usr/bin/env bash

set -Eeuo pipefail

script_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ -x "$script_root/server/adaptixserver" ]]; then
    install_dir="$script_root"
else
    install_dir="${ADAPTIX_INSTALL_DIR:-$script_root/install}"
fi

server_dir="$install_dir/server"
server_bin="$server_dir/adaptixserver"
default_profile="$server_dir/profile.yaml"
profile="$default_profile"
client_bin="$install_dir/client/AdaptixClient-x86_64.AppImage"
pid_file="$server_dir/server.pid"
log_file="$server_dir/server.log"

usage() {
    cat <<'EOF'
Usage: ./run-adaptix.sh [both|server|client|stop|status] [-profile PATH]

  both    Start the server, open the client, and stop the server when the
          client closes (default).
  server  Run only the server in the foreground.
  client  Open only the client.
  stop    Stop a server previously started by this launcher.
  status  Show whether that server is running.

Server options:
  -profile PATH   Use a specific Adaptix YAML profile with `both` or `server`.
                   Relative paths are resolved only from the current directory.

Environment:
  ADAPTIX_INSTALL_DIR=/path   Use a non-default installation directory.
  QT_QPA_PLATFORM=...        Override the default xcb Qt platform.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

server_pid() {
    [[ -f "$pid_file" ]] || return 1
    local pid
    read -r pid < "$pid_file"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null || return 1

    local running_exe expected_exe
    running_exe="$(readlink -f "/proc/$pid/exe" 2>/dev/null || true)"
    expected_exe="$(readlink -f "$server_bin" 2>/dev/null || true)"
    [[ -n "$running_exe" && "$running_exe" == "$expected_exe" ]] || return 1
    printf '%s\n' "$pid"
}

stop_server() {
    local pid
    if ! pid="$(server_pid)"; then
        rm -f "$pid_file"
        printf '[*] Adaptix server is not running.\n'
        return 0
    fi

    printf '[*] Stopping Adaptix server (PID %s)...\n' "$pid"
    kill "$pid"
    for _ in {1..50}; do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
    done
    rm -f "$pid_file"
    printf '[+] Adaptix server stopped.\n'
}

require_server() {
    [[ -x "$server_bin" ]] || fail "server is not installed; run ./install-fedora.sh"
    [[ -f "$profile" ]] || fail "missing server profile: $profile"
    command -v openssl >/dev/null 2>&1 || fail "OpenSSL is required to check server readiness"
}

require_client() {
    [[ -x "$client_bin" ]] || fail "client is not installed; run ./install-fedora.sh"
}

run_client() {
    printf '[*] Opening Adaptix client...\n'
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}" "$client_bin"
}

run_server_foreground() {
    printf '[*] Starting Adaptix server in the foreground...\n'
    cd "$server_dir"
    exec "$server_bin" -profile "$profile"
}

start_server_background() {
    local current_pid
    if current_pid="$(server_pid)"; then
        printf '[*] Adaptix server is already running (PID %s).\n' "$current_pid"
        return 1
    fi

    rm -f "$pid_file"
    printf '[*] Starting Adaptix server...\n'
    (
        cd "$server_dir"
        exec "$server_bin" -profile "$profile" >> "$log_file" 2>&1
    ) &
    local pid=$!
    printf '%s\n' "$pid" > "$pid_file"

    local interface port readiness_host
    interface="$(awk '/^[[:space:]]*interface:/ {gsub(/"/, "", $2); print $2; exit}' "$profile")"
    port="$(awk '/^[[:space:]]*port:/ {gsub(/[^0-9]/, "", $2); print $2; exit}' "$profile")"
    [[ -n "$port" ]] || port=4321
    case "$interface" in
        ""|0.0.0.0|::) readiness_host=127.0.0.1 ;;
        *) readiness_host="$interface" ;;
    esac

    for _ in {1..100}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            printf '%s\n' '--- server log ---' >&2
            tail -n 30 "$log_file" >&2 || true
            rm -f "$pid_file"
            fail "Adaptix server exited during startup"
        fi
        if openssl s_client \
            -connect "$readiness_host:$port" \
            -servername localhost \
            </dev/null >/dev/null 2>&1; then
            printf '[+] Adaptix server is ready on %s:%s (PID %s).\n' "$readiness_host" "$port" "$pid"
            return 0
        fi
        sleep 0.1
    done

    stop_server
    fail "timed out waiting for the Adaptix server; see $log_file"
}

action=both
if (($#)) && [[ "$1" != -* ]]; then
    action="$1"
    shift
fi

while (($#)); do
    case "$1" in
        -profile|--profile)
            (($# >= 2)) || fail "$1 requires a path"
            requested_profile="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

if [[ -n "${requested_profile:-}" ]]; then
    case "$action" in
        both|server) ;;
        *) fail "-profile can only be used with 'both' or 'server'" ;;
    esac

    profile="$(realpath -m -- "$requested_profile")"
fi

case "$action" in
    -h|--help) usage ;;
    both)
        require_server
        require_client
        started_here=false
        if start_server_background; then
            started_here=true
        fi
        cleanup() {
            trap - EXIT INT TERM HUP
            if [[ "$started_here" == true ]]; then
                started_here=false
                if server_pid >/dev/null; then
                    stop_server
                else
                    rm -f "$pid_file"
                fi
            fi
        }
        trap cleanup EXIT INT TERM HUP
        run_client
        ;;
    server)
        require_server
        run_server_foreground
        ;;
    client)
        require_client
        run_client
        ;;
    stop)
        stop_server
        ;;
    status)
        if pid="$(server_pid)"; then
            printf '[+] Adaptix server is running (PID %s).\n' "$pid"
        else
            printf '[*] Adaptix server is not running.\n'
        fi
        ;;
    *) usage >&2; exit 2 ;;
esac
