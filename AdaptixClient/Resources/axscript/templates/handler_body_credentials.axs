    // credentials.add / edit / remove
    var credId = event.credId;
    var count = event.count;
    var realm = event.realm || "";
    var host = event.host || "";
    var user = event.user || "";
    var credType = event.credType || "";
    ax.log("{{NAME}}: type=" + event.type
        + (credId !== undefined && credId !== null ? " credId=" + credId : "")
        + (count !== undefined && count !== null ? " count=" + count : "")
        + (realm ? " realm=" + realm : "")
        + (host ? " host=" + host : "")
        + (user ? " user=" + user : "")
        + (credType ? " credType=" + credType : ""));
