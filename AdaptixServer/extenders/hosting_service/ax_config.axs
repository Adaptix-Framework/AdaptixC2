/// Hosting Service - UI

let hostingDock = null;
let hostingTable = null;
let filesData = [];

// ============================================================================
// InitService - Called when service is loaded
// ============================================================================

function InitService() {
    createHostingDock();
    loadFiles();
}

// ============================================================================
// Data Handler - Receives data from server
// ============================================================================

function data_handler(data) {
    try {
        let json = JSON.parse(data);
        let msgType = json.type;

        if (msgType === "files") {
            filesData = json.data || [];
            refreshTable();
        }
        else if (msgType === "url") {
            ax.clipboard_set(json.data.url);
            ax.show_message("Hosting", "URL copied to clipboard:\n" + json.data.url);
        }
        else if (msgType === "event") {
            handleEvent(json.event, json.data);
        }
        else if (msgType === "error") {
            ax.show_message("Hosting Error", json.message);
        }
    } catch (e) {
        ax.log_error("Hosting: parse error: " + e);
    }
}

// ============================================================================
// Hosting Dock
// ============================================================================

function createHostingDock() {
    hostingDock = form.create_ext_dock("hosting_files", "Hosted Files", "");

    let mainLayout = form.create_vlayout();

    // Toolbar
    let toolbar = form.create_hlayout();

    let btnAdd = form.create_button("Add File");
    let btnDelete = form.create_button("Delete");
    let btnToggle = form.create_button("Toggle");
    let btnCopyURL = form.create_button("Copy URL");
    let btnRefresh = form.create_button("Refresh");

    toolbar.addWidget(btnAdd);
    toolbar.addWidget(btnToggle);
    toolbar.addWidget(btnCopyURL);
    toolbar.addWidget(btnDelete);
    toolbar.addWidget(form.create_hspacer());
    toolbar.addWidget(btnRefresh);

    let toolbarPanel = form.create_panel();
    toolbarPanel.setLayout(toolbar);
    mainLayout.addWidget(toolbarPanel);

    // Table
    hostingTable = form.create_table(["Path", "Filename", "MIME", "Size", "Downloads", "Status", "Created By", "Created"]);
    hostingTable.setSortingEnabled(true);
    hostingTable.setReadOnly(true);
    mainLayout.addWidget(hostingTable);

    hostingDock.setLayout(mainLayout);
    hostingDock.setSize(1000, 400);
    hostingDock.show();

    // Signals
    form.connect(btnAdd, "clicked", function() {
        showAddFileDialog();
    });

    form.connect(btnDelete, "clicked", function() {
        let rows = hostingTable.selectedRows();
        if (rows.length === 0) return;
        let fid = getFileIDByRow(rows[0]);
        if (fid) {
            if (ax.prompt_confirm("Delete File", "Remove this hosted file?")) {
                ax.service_command("Hosting", "delete", {id: fid});
            }
        }
    });

    form.connect(btnToggle, "clicked", function() {
        let rows = hostingTable.selectedRows();
        if (rows.length === 0) return;
        let fid = getFileIDByRow(rows[0]);
        if (fid) {
            ax.service_command("Hosting", "toggle", {id: fid});
        }
    });

    form.connect(btnCopyURL, "clicked", function() {
        let rows = hostingTable.selectedRows();
        if (rows.length === 0) return;
        let fid = getFileIDByRow(rows[0]);
        if (fid) {
            ax.service_command("Hosting", "copyurl", {id: fid});
        }
    });

    form.connect(btnRefresh, "clicked", function() {
        loadFiles();
    });
}

// ============================================================================
// Add File Dialog
// ============================================================================

function showAddFileDialog() {
    let dialog = form.create_dialog("Host File");
    dialog.setSize(600, 500);

    let pageLayout = form.create_vlayout();

    // --- File Selection ---
    let fileGrid = form.create_gridlayout();

    let txtFilePath = form.create_textline("");
    txtFilePath.setPlaceholder("Select a file to host...");
    txtFilePath.setReadOnly(true);
    let btnBrowse = form.create_button("Browse");
    let fileRow = form.create_hlayout();
    fileRow.addWidget(txtFilePath);
    fileRow.addWidget(btnBrowse);
    let filePanel = form.create_panel();
    filePanel.setLayout(fileRow);

    fileGrid.addWidget(form.create_label("File *"), 0, 0);
    fileGrid.addWidget(filePanel, 0, 1);

    let txtPath = form.create_textline("");
    txtPath.setPlaceholder("/downloads/payload.exe");
    fileGrid.addWidget(form.create_label("URL Path"), 1, 0);
    fileGrid.addWidget(txtPath, 1, 1);

    let txtMime = form.create_textline("application/octet-stream");
    fileGrid.addWidget(form.create_label("MIME Type"), 2, 0);
    fileGrid.addWidget(txtMime, 2, 1);

    let fileInner = form.create_panel();
    fileInner.setLayout(fileGrid);
    let grpFile = form.create_groupbox("File", false);
    grpFile.setPanel(fileInner);
    pageLayout.addWidget(grpFile);

    // --- Protections ---
    let protGrid = form.create_gridlayout();

    let chkOneShot = form.create_check("One-shot (auto-disable after first download)");
    protGrid.addWidget(chkOneShot, 0, 0, 1, 2);

    let chkEncrypt = form.create_check("AES-256-CBC encrypt content (key in X-Enc-Key header)");
    protGrid.addWidget(chkEncrypt, 1, 0, 1, 2);

    let txtUA = form.create_textline("");
    txtUA.setPlaceholder("Mozilla.*Windows.*  (regex, empty = allow all)");
    protGrid.addWidget(form.create_label("UA Filter"), 2, 0);
    protGrid.addWidget(txtUA, 2, 1);

    let spinMaxDL = form.create_spin();
    spinMaxDL.setRange(0, 999999);
    spinMaxDL.setValue(0);
    let maxDLRow = form.create_hlayout();
    maxDLRow.addWidget(spinMaxDL);
    maxDLRow.addWidget(form.create_label("  0 = unlimited"));
    let maxDLPanel = form.create_panel();
    maxDLPanel.setLayout(maxDLRow);
    protGrid.addWidget(form.create_label("Max Downloads"), 3, 0);
    protGrid.addWidget(maxDLPanel, 3, 1);

    let txtExpiry = form.create_textline("");
    txtExpiry.setPlaceholder("2025-12-31T23:59:59  (empty = no expiration)");
    protGrid.addWidget(form.create_label("Expires At"), 4, 0);
    protGrid.addWidget(txtExpiry, 4, 1);

    let protInner = form.create_panel();
    protInner.setLayout(protGrid);
    let grpProt = form.create_groupbox("Protections", false);
    grpProt.setPanel(protInner);
    pageLayout.addWidget(grpProt);

    pageLayout.addWidget(form.create_vspacer());

    dialog.setLayout(pageLayout);

    // Browse button
    let selectedFilePath = "";
    form.connect(btnBrowse, "clicked", function() {
        let path = ax.prompt_open_file("Select file to host", "All Files (*)");
        if (path) {
            selectedFilePath = path;
            txtFilePath.setText(path);
            // Auto-fill path from filename
            let parts = path.replace(/\\/g, "/").split("/");
            let fname = parts[parts.length - 1];
            if (!txtPath.text()) {
                txtPath.setText("/" + fname);
            }
        }
    });

    let accepted = dialog.exec();
    if (accepted === true) {
        if (!selectedFilePath) {
            ax.show_message("Error", "Please select a file to host");
            return;
        }

        let content = ax.file_read_base64(selectedFilePath);
        if (!content) {
            ax.show_message("Error", "Failed to read file");
            return;
        }

        let parts = selectedFilePath.replace(/\\/g, "/").split("/");
        let fname = parts[parts.length - 1];

        let req = {
            filename:      fname,
            content:       content,
            path:          txtPath.text(),
            mime_type:     txtMime.text(),
            one_shot:      chkOneShot.isChecked(),
            encrypted:     chkEncrypt.isChecked(),
            ua_filter:     txtUA.text(),
            max_downloads: spinMaxDL.value(),
            expires_at:    txtExpiry.text()
        };

        ax.service_command("Hosting", "add", req);
    }
}

// ============================================================================
// Table Helpers
// ============================================================================

function refreshTable() {
    if (!hostingTable) return;

    hostingTable.setRowCount(0);
    if (!filesData) return;

    for (let i = 0; i < filesData.length; i++) {
        let f = filesData[i];
        let created = f.created_at ? f.created_at : "";
        let status = f.enabled ? "Active" : "Disabled";
        let dlStr = String(f.downloads || 0);
        if (f.max_downloads > 0) {
            dlStr += " / " + String(f.max_downloads);
        }

        let sizeStr = formatSize(f.content_size || 0);

        hostingTable.addItem([
            f.path || "",
            f.filename || "",
            f.mime_type || "",
            sizeStr,
            dlStr,
            status,
            f.created_by || "",
            created
        ]);
    }
}

function getFileIDByRow(row) {
    if (!filesData || row < 0 || row >= filesData.length) return null;
    return filesData[row].id;
}

function formatSize(bytes) {
    if (bytes === 0) return "0 B";
    let units = ["B", "KB", "MB", "GB"];
    let i = 0;
    let size = bytes;
    while (size >= 1024 && i < units.length - 1) {
        size = size / 1024;
        i++;
    }
    if (i === 0) return String(size) + " B";
    return size.toFixed(1) + " " + units[i];
}

// ============================================================================
// Event Handling
// ============================================================================

function handleEvent(eventType, data) {
    if (eventType === "download") {
        // Reload file list to reflect updated download count
        loadFiles();
    }
}

// ============================================================================
// Data Loading
// ============================================================================

function loadFiles() {
    ax.service_command("Hosting", "list", {});
}
