server_version: "v2.0"

server_dir: AdaptixServer
client_dir: AdaptixClient
plugin_dir: extenders

dist_dir: dist
ext_dir: dist/extenders
axscript_dir: dist/axscripts

profile: dist/profile.yaml

systemd:
  name: adaptix
  user: root

# Host packages for: axtool … server|client build --install-deps (-d)
# Only apt is implemented (Debian/Ubuntu). Uses system apt sources (no extra repos).
deps:
  common:
    apt:
      - git
      - make
      - build-essential
      - openssl
      - pkg-config
  server:
    apt:
      - libssl-dev
      - zlib1g-dev
      - mingw-w64          # Windows agent cross-build
      - g++-mingw-w64
  client:
    apt:
      - cmake
      - libssl-dev
      - libgl1-mesa-dev
      - libxkbcommon-dev
      - libxcb-cursor-dev
      - libfontconfig1-dev
      - libfreetype6-dev
      - libdbus-1-dev
      - libsqlite3-dev
      # Distro Qt6 (may be < 6.9; install Qt 6.9+ manually if cmake fails)
      - qt6-base-dev
      - qt6-websockets-dev
      - qt6-declarative-dev
      - libqt6sql6-sqlite

# Default plugin sets installed via: server build (default) or ext install --packages -f
packages:
  # Beacon stack
  - source: ./AdaptixServer/extenders/beacon_listener_http
  - source: ./AdaptixServer/extenders/beacon_listener_smb
  - source: ./AdaptixServer/extenders/beacon_listener_tcp
  - source: ./AdaptixServer/extenders/beacon_listener_dns
  - source: ./AdaptixServer/extenders/beacon_agent

  # Gopher stack
  - source: ./AdaptixServer/extenders/gopher_listener_tcp
  - source: ./AdaptixServer/extenders/gopher_agent
