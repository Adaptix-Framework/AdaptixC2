    // tunnel.start / stop
    var agentId = event.agentId;
    var tunnelId = event.tunnelId;
    var port = event.port;
    var tunnelType = event.tunnelType;
    var info = event.info || "";
    var computer = event.computer || "";
    ax.log("{{NAME}}: type=" + event.type
        + (agentId !== undefined && agentId !== null ? " agent=" + agentId : "")
        + (tunnelId !== undefined && tunnelId !== null ? " tunnel=" + tunnelId : "")
        + (port !== undefined && port !== null ? " port=" + port : "")
        + (tunnelType !== undefined && tunnelType !== null && tunnelType !== "" ? " tunnelType=" + tunnelType : "")
        + (computer ? " computer=" + computer : "")
        + (info ? " info=" + info : ""));
