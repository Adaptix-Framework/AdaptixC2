#!/usr/bin/env bash

GREEN=$(printf '\033[1;32m')
YELLOW=$(printf '\033[1;33m')
RED=$(printf '\033[1;31m')
NC=$(printf '\033[0m')

GO_VERSION='1.26.5'

# Map uname -m to the Go download filename (linux-<arch>.tar.gz).
go_arch_from_uname() {
    case "$(uname -m)" in
        x86_64|amd64)  echo amd64 ;;
        aarch64|arm64) echo arm64 ;;
        armv7l|armv6l) echo armv6l ;;
        i386|i686)     echo 386 ;;
        ppc64le)       echo ppc64le ;;
        s390x)         echo s390x ;;
        riscv64)       echo riscv64 ;;
        loongarch64)   echo loong64 ;;
        *)             return 1 ;;
    esac
}

ERROR_FILE="$(date "+%d.%m.%Y_%H-%M-%S")-error.log"

SERVER_DEPENDENCIES=(
    wget
    git
    mingw-w64
    make
    gcc
    g++
    g++-mingw-w64
)

CLIENT_DEPENDENCIES=(
    wget
    git
    gcc
    g++
    build-essential
    make
    cmake
    mingw-w64
    g++-mingw-w64
    libssl-dev
    qt6-base-dev
    qt6-base-private-dev
    libxkbcommon-dev
    qt6-websockets-dev
    qt6-declarative-dev
    qt6-svg-dev
)

report_step() {
    echo
    echo "${YELLOW}[STEP]${NC} $1..."
}

report_success() {
    echo "${GREEN}[SUCCESS]${NC} $1!"
}

report_fail() {
    echo "${RED}[FAIL]${NC} $1!"
}

packets_install() {
    local -n packages_list=$1

    for package in ${packages_list[@]}; do
        if apt install -y $package > /dev/null; then
            report_success "The \"$package\" package has been successfully installed into the system"
        else
            report_fail "An error occurred during the installation of the \"$package\" package"
        fi
    done
}

repo_index_update() {
    report_step "Updating the repository index"

    if apt update > /dev/null; then
        report_success "The repository index has been successfully updated"
    else
        report_fail "An error occurred while updating the repository index"
    fi
}

server_packets_install() {
    report_step "Installing the packages dependencies for the server"
    packets_install SERVER_DEPENDENCIES
}

actual_go_version_install() {
    report_step "Downloading Go $GO_VERSION for this architecture"

    if ! GO_ARCH="$(go_arch_from_uname)"; then
        report_fail "Unsupported architecture: $(uname -m)"
        exit 1
    fi

    DOWNLOAD_PATH="/tmp/go${GO_VERSION}.linux-${GO_ARCH}.tar.gz"
    DOWNLOAD_URL="https://go.dev/dl/go${GO_VERSION}.linux-${GO_ARCH}.tar.gz"

    if wget "$DOWNLOAD_URL" -O "$DOWNLOAD_PATH"; then
        report_success "The Go distribution for version $GO_VERSION ($GO_ARCH) has been successfully downloaded to \"$DOWNLOAD_PATH\""
    else
        report_fail "An error occurred while downloading the Go distribution version $GO_VERSION to \"$DOWNLOAD_PATH\""
        exit 1
    fi

    if rm -rf /usr/local/go /usr/local/bin/go; then
        report_success "The old version of Go has been successfully removed"
    else
        report_fail "An error occurred while deleting the old version of Go"
        exit 1
    fi

    if tar -xzf "$DOWNLOAD_PATH" -C /usr/local; then
        report_success "The downloaded Go distribution version $GO_VERSION has been successfully extracted to \"/usr/local/go\""
    else
        report_fail "An error occurred while extracting the Go distribution of version $GO_VERSION to \"/usr/local/go\""
        exit 1
    fi

    if ln -sf /usr/local/go/bin/go /usr/local/bin/go; then
        report_success "A symbolic link to the PATH directory for Go version $GO_VERSION has been created successfully"
    else
        report_fail "An error occurred while creating a symbolic link to the PATH directory for Go version $GO_VERSION"
        exit 1
    fi

    if /usr/local/go/bin/go version; then
        report_success "Go $GO_VERSION ($GO_ARCH) is executable on this host"
    else
        report_fail "Installed Go binary is not executable on this architecture ($(uname -m))"
        exit 1
    fi

    report_step "Downloading the latest version of Go, which includes Windows 7 support for Gopher Agent"

    if git clone https://github.com/Adaptix-Framework/go-win7 /tmp/go-win7; then
        report_success "The latest version of Go, with Windows 7 support for Gopher Agent, has been successfully downloaded"
    else
        report_fail "An error occurred while downloading the latest version of Go with Windows 7 support for Gopher Agent"
    fi

    if [ -d /usr/lib/go-win7 ]; then
        if rm -rf /usr/lib/go-win7; then
            report_success "An existing directory \"/usr/lib/go-win7\" has been successfully deleted"
        else
            report_fail "An existing directory \"/usr/lib/go-win7\" has been found! An error occurred while attempting to delete it"
        fi
    fi

    if mv /tmp/go-win7 /usr/lib/; then
        report_success "The latest version of Go with Windows 7 support has been successfully installed"
    else
        report_fail "An error occurred while installing the current version of Go with Windows 7 support for Gopher Agent"
    fi
}

client_packets_install() {
    report_step "Installing the packages dependencies for the client"
    packets_install CLIENT_DEPENDENCIES
}

server_part() {
    repo_index_update
    server_packets_install
    actual_go_version_install
}

client_part() {
    repo_index_update
    client_packets_install
}

exec 2> "$ERROR_FILE"

if [[ $EUID != 0 ]]; then
    report_fail "This script must be executed as root"
    exit 1
fi

if [[ $# != 1 ]]; then
    echo "Usage: pre_install_linux_all.sh <server|client|all>"
    exit 1
else
    case "$1" in
        server)
            server_part
            ;;
        client)
            client_part
            ;;
        all)
            server_part
            client_part
            ;;
        *)
            echo "Usage: pre_install_linux_all.sh <server|client|all>"
            exit 1
            ;;
    esac
fi
