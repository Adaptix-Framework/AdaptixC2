extenders:
  - name: beacon_agent
    version: 1.0.0
    type: agent
    description: "beacon agent plugin"
    author: Adaptix-Framework
    min_server_version: "v2.0"
    requires: [beacon_listener_http]
    deps:
      apt:
        - mingw-w64
        - g++-mingw-w64
    build:
      - make
    release:
      dir: dist
