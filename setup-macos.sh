#!/usr/bin/env bash
# Author: AuxGrep
# This utility tested on Mac Silicon latest version 04th June 2026

set -euo pipefail

echo "[*] installing make, cmake, and qt@6 "
brew install make cmake openssl qt@6

echo "[*] Detecting architecture..."

ARCH=$(uname -m)

if [[ "$ARCH" == "arm64" ]]; then
    HOMEBREW_PREFIX="/opt/homebrew"
elif [[ "$ARCH" == "x86_64" ]]; then
    HOMEBREW_PREFIX="/usr/local"
else
    echo "[!] Unsupported architecture: $ARCH"
    exit 1
fi

echo "[+] Homebrew prefix: $HOMEBREW_PREFIX"

if ! command -v brew >/dev/null 2>&1; then
    echo "[!] Homebrew is required."
    exit 1
fi

echo "[*] Installing Qt6 dependencies..."

brew install \
    qtbase \
    qtdeclarative \
    qtsvg \
    qtwebsockets \
    openssl \
    cmake

echo "[*] Locating Qt6 modules..."

QT6_DIR=$(find "$HOMEBREW_PREFIX" -name Qt6Config.cmake 2>/dev/null | head -1)
QT6SVG_DIR=$(find "$HOMEBREW_PREFIX" -name Qt6SvgConfig.cmake 2>/dev/null | head -1)
QT6WS_DIR=$(find "$HOMEBREW_PREFIX" -name Qt6WebSocketsConfig.cmake 2>/dev/null | head -1)
QT6QML_DIR=$(find "$HOMEBREW_PREFIX" -name Qt6QmlConfig.cmake 2>/dev/null | head -1)

if [[ -z "$QT6_DIR" ]]; then
    echo "[!] Qt6Config.cmake not found"
    exit 1
fi

QT6_DIR=$(dirname "$QT6_DIR")
QT6SVG_DIR=$(dirname "$QT6SVG_DIR")
QT6WS_DIR=$(dirname "$QT6WS_DIR")
QT6QML_DIR=$(dirname "$QT6QML_DIR")

echo "[+] Qt6_DIR=$QT6_DIR"
echo "[+] Qt6Svg_DIR=$QT6SVG_DIR"
echo "[+] Qt6WebSockets_DIR=$QT6WS_DIR"
echo "[+] Qt6Qml_DIR=$QT6QML_DIR"

echo "[*] Configuring AdaptixClient..."

cd AdaptixClient

cmake . \
  -DQt6_DIR="$QT6_DIR" \
  -DQt6Svg_DIR="$QT6SVG_DIR" \
  -DQt6WebSockets_DIR="$QT6WS_DIR" \
  -DQt6Qml_DIR="$QT6QML_DIR"

echo "[+] Configuration completed"

cat <<EOF

Build with:

    make

Run with:

    export QT_PLUGIN_PATH=${HOMEBREW_PREFIX}/Cellar/qtbase/$(brew list --versions qtbase | awk '{print $2}')/share/qt/plugins
    export QT_QPA_PLATFORM_PLUGIN_PATH=\$QT_PLUGIN_PATH/platforms
    ./AdaptixClient

EOF
