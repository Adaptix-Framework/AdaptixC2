    // pivot.create / remove
    var pivotId = event.pivotId;
    var parentId = event.parentAgentId;
    var childId = event.childAgentId;
    var name = event.pivotName || "";
    ax.log("{{NAME}}: type=" + event.type
        + (pivotId !== undefined && pivotId !== null ? " pivot=" + pivotId : "")
        + (parentId !== undefined && parentId !== null ? " parent=" + parentId : "")
        + (childId !== undefined && childId !== null ? " child=" + childId : "")
        + (name ? " name=" + name : ""));
