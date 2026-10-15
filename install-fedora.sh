#!/usr/bin/env bash

set -Eeuo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$repo_root/scripts/install-fedora.sh" "$@"
