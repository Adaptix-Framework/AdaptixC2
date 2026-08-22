    // listener.start / listener.stop
    var name = event.listenerName || event.listener || "";
    var type = event.listenerType || "";
    ax.log("{{NAME}}: type=" + event.type
        + " listener=" + name
        + (type ? " listenerType=" + type : ""));
