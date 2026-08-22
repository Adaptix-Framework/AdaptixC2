    // screenshot.add / remove
    var agentId = event.agentId;
    var screenId = event.screenId;
    var note = event.note || "";
    var computer = event.computer || "";
    ax.log("{{NAME}}: type=" + event.type
        + (agentId !== undefined && agentId !== null ? " agent=" + agentId : "")
        + (screenId !== undefined && screenId !== null ? " screen=" + screenId : "")
        + (computer ? " computer=" + computer : "")
        + (note ? " note=" + note : ""));
