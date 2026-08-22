    // upload.start / finish / remove
    var agentId = event.agentId;
    var fileId = event.fileId;
    var filename = event.filename || event.fileName || event.remotePath || "";
    var fileSize = event.fileSize;
    var canceled = event.canceled;
    ax.log("{{NAME}}: type=" + event.type
        + (agentId !== undefined && agentId !== null ? " agent=" + agentId : "")
        + (fileId !== undefined && fileId !== null ? " file=" + fileId : "")
        + (filename ? " path=" + filename : "")
        + (fileSize !== undefined && fileSize !== null ? " size=" + fileSize : "")
        + (canceled !== undefined ? " canceled=" + canceled : "")
        + (event.count !== undefined && event.count !== null ? " count=" + event.count : ""));
