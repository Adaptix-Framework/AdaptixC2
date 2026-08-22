    // client.connect / client.disconnect
    var user = event.username || event.client || "";
    ax.log("{{NAME}}: type=" + event.type + " user=" + user);
