extenders:
  - name: gopher_agent
    version: 1.0.0
    type: agent
    description: "gopher agent plugin"
    author: Adaptix-Framework
    min_server_version: "v2.0"
    requires: [gopher_listener_tcp]
    deps:
      apt:
        - mingw-w64
        - g++-mingw-w64
    build:
      - make
    release:
      dir: dist
