extenders:
  - name: beacon_listener_smb
    version: 1.0.0
    type: listener
    description: "beacon smb listener plugin"
    author: Adaptix-Framework
    min_server_version: "v2.0"
    build:
      - make
    release:
      dir: dist
