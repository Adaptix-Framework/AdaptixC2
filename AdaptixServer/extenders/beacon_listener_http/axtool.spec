extenders:
  - name: beacon_listener_http
    version: 1.0.0
    type: listener
    description: "beacon http listener plugin"
    author: Adaptix-Framework
    min_server_version: "v2.0"
    build:
      - make
    release:
      dir: dist
