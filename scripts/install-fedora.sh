#!/usr/bin/env bash

set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: ./install-fedora.sh [--no-build] [--prefix PATH]

Build and install the Adaptix client, server, and standard extenders into a
self-contained local directory. The default is ./install. If a built
Extension-Kit checkout is present, its runtime files are installed and loaded
through the server profile as well.

Options:
  --no-build      Install existing build artifacts without rebuilding.
  --prefix PATH   Choose another installation directory.
  -h, --help      Show this help.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
install_dir="${ADAPTIX_INSTALL_DIR:-$repo_root/install}"
extension_kit_source="${ADAPTIX_EXTENSION_KIT_DIR:-$repo_root/Extension-Kit}"
build=true

while (($#)); do
    case "$1" in
        --no-build)
            build=false
            shift
            ;;
        --prefix)
            (($# >= 2)) || fail "--prefix requires a path"
            install_dir="$(realpath -m -- "$2")"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

client_source="$repo_root/AdaptixClient/client-dist/AdaptixClient-x86_64.AppImage"
server_source="$repo_root/AdaptixServer/server-dist"

if [[ "$build" == true ]]; then
    "$repo_root/scripts/build-fedora.sh" all
fi

[[ -x "$client_source" ]] || fail "missing client build: $client_source"
[[ -x "$server_source/adaptixserver" ]] || fail "missing server build: $server_source/adaptixserver"
[[ -d "$server_source/extenders" ]] || fail "missing built extenders: $server_source/extenders"
command -v openssl >/dev/null 2>&1 || fail "install OpenSSL with: sudo dnf install openssl"

client_dir="$install_dir/client"
server_dir="$install_dir/server"
mkdir -p "$client_dir" "$server_dir/extenders"

printf '[*] Installing client...\n'
install -m 0755 "$client_source" "$client_dir/AdaptixClient-x86_64.AppImage"

printf '[*] Installing server and extenders...\n'
install -m 0755 "$server_source/adaptixserver" "$server_dir/adaptixserver"
install -m 0644 "$server_source/404page.html" "$server_dir/404page.html"
cp -a "$server_source/extenders/." "$server_dir/extenders/"

extension_kit_installed=false
if [[ -f "$extension_kit_source/extension-kit.axs" ]]; then
    if ! find "$extension_kit_source" -type f -path '*/_bin/*.o' -print -quit | grep -q .; then
        fail "Extension Kit is present but not built; run make in $extension_kit_source"
    fi

    printf '[*] Installing the built Extension Kit...\n'
    while IFS= read -r -d '' source_file; do
        relative_path="${source_file#"$extension_kit_source"/}"
        install -D -m 0644 "$source_file" "$server_dir/Extension-Kit/$relative_path"
    done < <(
        find "$extension_kit_source" -type f \
            \( -name '*.axs' -o -path '*/_bin/*' \) -print0
    )
    extension_kit_installed=true
fi

if [[ ! -f "$server_dir/profile.yaml" ]]; then
    password="$(openssl rand -hex 24)"
    sed \
        -e 's/interface: "0.0.0.0"/interface: "127.0.0.1"/' \
        -e "s/password: \"pass\"/password: \"$password\"/" \
        "$server_source/profile.yaml" > "$server_dir/profile.yaml"
    chmod 0600 "$server_dir/profile.yaml"
    profile_created=true
else
    profile_created=false
    password="$(awk -F'"' '/^[[:space:]]*password:/ {print $2; exit}' "$server_dir/profile.yaml")"
    printf '[*] Preserving existing profile and password.\n'
fi

only_password="$(awk '/^[[:space:]]*only_password:/ {print $2; exit}' "$server_dir/profile.yaml")"

if [[ "$extension_kit_installed" == true ]] &&
   ! grep -Eq '^[[:space:]]*-[[:space:]]*"Extension-Kit/extension-kit\.axs"[[:space:]]*$' "$server_dir/profile.yaml"; then
    if grep -Eq '^[[:space:]]*axscripts:[[:space:]]*$' "$server_dir/profile.yaml"; then
        sed -i '/^[[:space:]]*axscripts:[[:space:]]*$/a\    - "Extension-Kit/extension-kit.axs"' "$server_dir/profile.yaml"
    else
        sed -i '/^[[:space:]]*access_token_live_hours:/i\  axscripts:\n    - "Extension-Kit/extension-kit.axs"' "$server_dir/profile.yaml"
    fi
    printf '[+] Extension Kit enabled in the server profile.\n'
fi

if [[ ! -f "$server_dir/server.rsa.crt" || ! -f "$server_dir/server.rsa.key" ]]; then
    printf '[*] Generating a local TLS certificate...\n'
    openssl req -x509 -nodes -newkey rsa:3072 \
        -keyout "$server_dir/server.rsa.key" \
        -out "$server_dir/server.rsa.crt" \
        -days 825 \
        -subj '/CN=localhost' \
        -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' \
        >/dev/null 2>&1
    chmod 0600 "$server_dir/server.rsa.key"
    chmod 0644 "$server_dir/server.rsa.crt"
fi

install -m 0755 "$repo_root/run-adaptix.sh" "$install_dir/run-adaptix.sh"

cat > "$install_dir/connection.txt" <<EOF
Adaptix local connection
Host: 127.0.0.1
Port: 4321
Endpoint: /endpoint
EOF
if [[ "$only_password" == true ]]; then
    cat >> "$install_dir/connection.txt" <<EOF
Authentication: password-only
Nickname: any value
Password: $password
EOF
else
    cat >> "$install_dir/connection.txt" <<'EOF'
Authentication: operator credentials from server/profile.yaml
EOF
fi
chmod 0600 "$install_dir/connection.txt"

printf '\n[+] Adaptix installation completed: %s\n' "$install_dir"
printf '[+] Connection details: %s\n' "$install_dir/connection.txt"
if [[ "$profile_created" == true ]]; then
    printf '[+] A unique password and local-only server profile were created.\n'
fi
if [[ "$only_password" == true ]]; then
    printf '[+] Login: use any nickname and the password stored in connection.txt.\n'
    printf '[+] The upstream example operator credentials are not active in password-only mode.\n'
else
    printf '[+] Login: use an operator name and password from server/profile.yaml.\n'
fi
printf '\nRun both server and client with:\n  %s/run-adaptix.sh\n' "$install_dir"
