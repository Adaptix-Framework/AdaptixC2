    // target.add / edit / remove
    var targetId = event.targetId;
    var count = event.count;
    var computer = event.computer || "";
    var domain = event.domain || "";
    var address = event.address || "";
    var os = event.os || "";
    ax.log("{{NAME}}: type=" + event.type
        + (targetId !== undefined && targetId !== null ? " target=" + targetId : "")
        + (count !== undefined && count !== null ? " count=" + count : "")
        + (computer ? " computer=" + computer : "")
        + (address ? " address=" + address : "")
        + (domain ? " domain=" + domain : "")
        + (os ? " os=" + os : ""));
