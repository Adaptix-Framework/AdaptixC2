/// Phishing Service - UI

let campaignsDock = null;
let resultsDock = null;
let campaignsTable = null;
let resultsTable = null;
let campaignFilter = null;
let campaignsData = [];
let allResults = {};
let activePreview = null;

// ============================================================================
// InitService - Called when service is loaded
// ============================================================================

function InitService() {
    createCampaignsDock();
    createResultsDock();
    loadInitialData();
}

// ============================================================================
// Data Handler - Receives data from server
// ============================================================================

function data_handler(data) {
    try {
        let json = JSON.parse(data);
        let msgType = json.type;

        if (msgType === "campaigns") {
            campaignsData = json.data || [];
            refreshCampaignsTable();
        }
        else if (msgType === "targets") {
            showTargetsDialog(json.data.campaign_id, json.data.targets || []);
        }
        else if (msgType === "results") {
            let cid = json.data.campaign_id;
            allResults[cid] = json.data.results || [];
            refreshResultsTable();
        }
        else if (msgType === "event") {
            handleEvent(json.event, json.data);
        }
        else if (msgType === "error") {
            ax.show_message("Phishing Error", json.message);
        }
        else if (msgType === "templates") {
            cachedTemplates = json.data || [];
        }
        else if (msgType === "landers") {
            cachedLanders = json.data || [];
        }
        else if (msgType === "preview") {
            if (activePreview) {
                activePreview.setHtml(json.data.html);
            }
        }
        else if (msgType === "export") {
            let filename = "phishing_results_" + json.data.campaign_id + ".csv";
            let path = ax.prompt_save_file(filename, "Export Results", "CSV Files (*.csv)");
            if (path) {
                ax.file_write_text(path, json.data.csv, false);
            }
        }
    } catch (e) {
        ax.log_error("Phishing: parse error: " + e);
    }
}

// ============================================================================
// Campaigns Dock
// ============================================================================

let cachedTemplates = [];
let cachedLanders = [];

function createCampaignsDock() {
    campaignsDock = form.create_ext_dock("phishing_campaigns", "Phishing Campaigns", "");

    let mainLayout = form.create_vlayout();

    // Toolbar
    let toolbar = form.create_hlayout();

    let btnNew = form.create_button("New Campaign");
    let btnStart = form.create_button("Start");
    let btnStop = form.create_button("Stop");
    let btnDelete = form.create_button("Delete");
    let btnTargets = form.create_button("Targets");
    let btnRefresh = form.create_button("Refresh");

    toolbar.addWidget(btnNew);
    toolbar.addWidget(btnStart);
    toolbar.addWidget(btnStop);
    toolbar.addWidget(btnTargets);
    toolbar.addWidget(btnDelete);
    toolbar.addWidget(form.create_hspacer());
    toolbar.addWidget(btnRefresh);

    let toolbarPanel = form.create_panel();
    toolbarPanel.setLayout(toolbar);
    mainLayout.addWidget(toolbarPanel);

    // Table
    campaignsTable = form.create_table(["Name", "Status", "Targets", "Sent", "Opened", "Clicked", "Submitted", "Errors", "Created By"]);
    campaignsTable.setSortingEnabled(true);
    campaignsTable.setReadOnly(true);
    mainLayout.addWidget(campaignsTable);

    campaignsDock.setLayout(mainLayout);
    campaignsDock.setSize(900, 400);
    campaignsDock.show();

    // Signals
    form.connect(btnNew, "clicked", function() {
        ax.service_command("Phishing", "templates_list", {});
        ax.service_command("Phishing", "landers_list", {});
        // Small delay to let templates/landers load before showing dialog
        event.on_timeout(function() { showNewCampaignDialog(); }, 1);
    });

    form.connect(btnStart, "clicked", function() {
        let rows = campaignsTable.selectedRows();
        if (rows.length === 0) return;
        let cid = getCampaignIDByRow(rows[0]);
        if (cid) {
            if (ax.prompt_confirm("Start Campaign", "Send emails for this campaign?")) {
                ax.service_command("Phishing", "campaign_start", {id: cid});
            }
        }
    });

    form.connect(btnStop, "clicked", function() {
        let rows = campaignsTable.selectedRows();
        if (rows.length === 0) return;
        let cid = getCampaignIDByRow(rows[0]);
        if (cid) {
            ax.service_command("Phishing", "campaign_stop", {id: cid});
        }
    });

    form.connect(btnDelete, "clicked", function() {
        let rows = campaignsTable.selectedRows();
        if (rows.length === 0) return;
        let cid = getCampaignIDByRow(rows[0]);
        if (cid) {
            if (ax.prompt_confirm("Delete Campaign", "Delete this campaign and all its data?")) {
                ax.service_command("Phishing", "campaign_delete", {id: cid});
            }
        }
    });

    form.connect(btnTargets, "clicked", function() {
        let rows = campaignsTable.selectedRows();
        if (rows.length === 0) return;
        let cid = getCampaignIDByRow(rows[0]);
        if (cid) {
            ax.service_command("Phishing", "targets_list", {campaign_id: cid});
        }
    });

    form.connect(btnRefresh, "clicked", function() {
        loadInitialData();
    });

    form.connect(campaignsTable, "cellDoubleClicked", function(row, col) {
        let cid = getCampaignIDByRow(row);
        if (cid) {
            ax.service_command("Phishing", "results_list", {campaign_id: cid});
        }
    });
}

function refreshCampaignsTable() {
    if (!campaignsTable) return;

    campaignsTable.setRowCount(0);
    if (!campaignsData) return;

    for (let i = 0; i < campaignsData.length; i++) {
        let c = campaignsData[i];
        let created = c.created_at ? ax.format_time("yyyy-MM-dd HH:mm", c.created_at) : "";
        campaignsTable.addItem([
            c.name || "",
            c.status || "",
            String(c.total_targets || 0),
            String(c.sent || 0),
            String(c.opened || 0),
            String(c.clicked || 0),
            String(c.submitted || 0),
            String(c.errors || 0),
            c.created_by || ""
        ]);
    }

    // Update filter combo in results dock
    updateCampaignFilter();
}

function getCampaignIDByRow(row) {
    if (!campaignsData || row < 0 || row >= campaignsData.length) return null;
    return campaignsData[row].id;
}

// ============================================================================
// New Campaign Dialog
// ============================================================================

function showNewCampaignDialog() {
    let dialog = form.create_dialog("New Phishing Campaign");
    dialog.setSize(920, 700);

    // ======================= Form fields =======================

    let txtName = form.create_textline("");
    txtName.setPlaceholder("Q1-2025 Password Audit - Finance Dept");
    let txtSubject = form.create_textline("");
    txtSubject.setPlaceholder("Action Required: Your password expires in 24 hours");
    let txtSenderEmail = form.create_textline("");
    txtSenderEmail.setPlaceholder("it-security@contoso.com");
    let txtSenderName = form.create_textline("");
    txtSenderName.setPlaceholder("IT Service Desk");

    let txtSmtpHost = form.create_textline("");
    txtSmtpHost.setPlaceholder("smtp.gmail.com");
    let spinSmtpPort = form.create_spin();
    spinSmtpPort.setRange(1, 65535);
    spinSmtpPort.setValue(587);
    let txtSmtpUser = form.create_textline("");
    txtSmtpUser.setPlaceholder("relay@yourdomain.com");
    let txtSmtpPass = form.create_textline("");
    txtSmtpPass.setPlaceholder("App password or SMTP credential");
    let chkSmtpTLS = form.create_check("Enable TLS");
    chkSmtpTLS.setChecked(true);

    let cmbTemplate = form.create_combo();
    let cmbLander = form.create_combo();
    if (cachedTemplates && cachedTemplates.length > 0) {
        for (let i = 0; i < cachedTemplates.length; i++) cmbTemplate.addItem(cachedTemplates[i]);
    }
    if (cachedLanders && cachedLanders.length > 0) {
        for (let i = 0; i < cachedLanders.length; i++) cmbLander.addItem(cachedLanders[i]);
    }

    let txtBaseURL = form.create_textline("");
    txtBaseURL.setPlaceholder("https://portal-auth.contoso.com");
    let txtRedirectURL = form.create_textline("https://login.microsoftonline.com");
    let chkTrackOpens = form.create_check("Track email opens (1x1 tracking pixel)");
    chkTrackOpens.setChecked(true);
    let chkTrackClicks = form.create_check("Track link clicks (redirect through server)");
    chkTrackClicks.setChecked(true);
    let spinDelay = form.create_spin();
    spinDelay.setRange(0, 300);
    spinDelay.setValue(3);

    // Preview browser
    let previewBrowser = form.create_textbrowser();
    activePreview = previewBrowser;

    // ======================= Layout =======================

    let pageLayout = form.create_vlayout();

    // --- Campaign Identity ---
    let identGrid = form.create_gridlayout();
    identGrid.addWidget(form.create_label("Name *"), 0, 0);
    identGrid.addWidget(txtName, 0, 1);
    identGrid.addWidget(form.create_label("Subject *"), 1, 0);
    identGrid.addWidget(txtSubject, 1, 1);

    let identInner = form.create_panel();
    identInner.setLayout(identGrid);
    let grpIdent = form.create_groupbox("Campaign Identity", false);
    grpIdent.setPanel(identInner);
    pageLayout.addWidget(grpIdent);

    // --- Sender ---
    let senderGrid = form.create_gridlayout();
    senderGrid.addWidget(form.create_label("Email *"), 0, 0);
    senderGrid.addWidget(txtSenderEmail, 0, 1);
    senderGrid.addWidget(form.create_label("Display Name"), 1, 0);
    senderGrid.addWidget(txtSenderName, 1, 1);

    let senderInner = form.create_panel();
    senderInner.setLayout(senderGrid);
    let grpSender = form.create_groupbox("Sender (From)", false);
    grpSender.setPanel(senderInner);
    pageLayout.addWidget(grpSender);

    // --- SMTP Server ---
    let smtpGrid = form.create_gridlayout();
    smtpGrid.addWidget(form.create_label("Host *"), 0, 0);
    smtpGrid.addWidget(txtSmtpHost, 0, 1);
    smtpGrid.addWidget(form.create_label("Port"), 1, 0);
    let portRow = form.create_hlayout();
    portRow.addWidget(spinSmtpPort);
    portRow.addWidget(form.create_label("  587=STARTTLS  465=SMTPS  25=Plain"));
    let portPanel = form.create_panel();
    portPanel.setLayout(portRow);
    smtpGrid.addWidget(portPanel, 1, 1);
    smtpGrid.addWidget(form.create_label("Username"), 2, 0);
    smtpGrid.addWidget(txtSmtpUser, 2, 1);
    smtpGrid.addWidget(form.create_label("Password"), 3, 0);
    smtpGrid.addWidget(txtSmtpPass, 3, 1);
    smtpGrid.addWidget(chkSmtpTLS, 4, 1);

    let smtpInner = form.create_panel();
    smtpInner.setLayout(smtpGrid);
    let grpSmtp = form.create_groupbox("SMTP Server", false);
    grpSmtp.setPanel(smtpInner);
    pageLayout.addWidget(grpSmtp);

    // --- Content & Preview (splitter) ---
    let contentGrid = form.create_gridlayout();
    contentGrid.addWidget(form.create_label("Email Template"), 0, 0);
    contentGrid.addWidget(cmbTemplate, 0, 1);
    contentGrid.addWidget(form.create_label("Landing Page"), 1, 0);
    contentGrid.addWidget(cmbLander, 1, 1);

    // Template descriptions table
    let tplDesc = form.create_table(["Template", "Scenario", "Best paired with"]);
    tplDesc.setReadOnly(true);
    tplDesc.setSortingEnabled(false);
    tplDesc.setHeadersVisible(true);
    tplDesc.addItem(["password_expiry",        "Password expiration alert",       "microsoft_login"]);
    tplDesc.addItem(["shared_document",        "SharePoint file share",           "microsoft_login"]);
    tplDesc.addItem(["voicemail_notification",  "Teams voicemail received",        "microsoft_login"]);
    tplDesc.addItem(["helpdesk_ticket",        "IT support ticket opened",        "okta_login"]);
    tplDesc.addItem(["mfa_setup",              "MFA enrollment required",         "okta_login"]);
    tplDesc.addItem(["default_email",          "Generic document review",         "default_login"]);

    // Left side: combos + reference table
    let leftLayout = form.create_vlayout();
    let contentGridPanel = form.create_panel();
    contentGridPanel.setLayout(contentGrid);
    leftLayout.addWidget(contentGridPanel);
    leftLayout.addWidget(tplDesc);

    let leftPanel = form.create_panel();
    leftPanel.setLayout(leftLayout);

    // Right side: HTML preview
    let rightLayout = form.create_vlayout();
    rightLayout.addWidget(form.create_label("Preview"));
    rightLayout.addWidget(previewBrowser);

    let rightPanel = form.create_panel();
    rightPanel.setLayout(rightLayout);

    // Splitter: left controls | right preview
    let contentSplitter = form.create_hsplitter();
    contentSplitter.addPage(leftPanel);
    contentSplitter.addPage(rightPanel);
    contentSplitter.setSizes([320, 540]);

    let contentSplitLayout = form.create_vlayout();
    contentSplitLayout.addWidget(contentSplitter);

    let contentInner = form.create_panel();
    contentInner.setLayout(contentSplitLayout);
    let grpContent = form.create_groupbox("Content & Preview", false);
    grpContent.setPanel(contentInner);
    pageLayout.addWidget(grpContent);

    // Connect combos to preview
    form.connect(cmbTemplate, "currentTextChanged", function(text) {
        if (text) ax.service_command("Phishing", "template_preview", {type: "template", name: text});
    });
    form.connect(cmbLander, "currentTextChanged", function(text) {
        if (text) ax.service_command("Phishing", "template_preview", {type: "lander", name: text});
    });

    // Load initial preview for the first selected template
    if (cmbTemplate.currentText()) {
        ax.service_command("Phishing", "template_preview", {type: "template", name: cmbTemplate.currentText()});
    }

    // --- Tracking & Delivery ---
    let trackGrid = form.create_gridlayout();
    trackGrid.addWidget(form.create_label("Base URL *"), 0, 0);
    trackGrid.addWidget(txtBaseURL, 0, 1);
    trackGrid.addWidget(form.create_label("Redirect URL"), 1, 0);
    trackGrid.addWidget(txtRedirectURL, 1, 1);
    trackGrid.addWidget(chkTrackOpens, 2, 1);
    trackGrid.addWidget(chkTrackClicks, 3, 1);
    trackGrid.addWidget(form.create_label("Send Delay (s)"), 4, 0);
    let delayRow = form.create_hlayout();
    delayRow.addWidget(spinDelay);
    delayRow.addWidget(form.create_label("  Seconds between each email sent"));
    let delayPanel = form.create_panel();
    delayPanel.setLayout(delayRow);
    trackGrid.addWidget(delayPanel, 4, 1);

    let trackInner = form.create_panel();
    trackInner.setLayout(trackGrid);
    let grpTrack = form.create_groupbox("Tracking & Delivery", false);
    grpTrack.setPanel(trackInner);
    pageLayout.addWidget(grpTrack);

    // --- Spacer at bottom ---
    pageLayout.addWidget(form.create_vspacer());

    // --- Scrollable container ---
    let scrollContent = form.create_panel();
    scrollContent.setLayout(pageLayout);
    let scrollArea = form.create_scrollarea();
    scrollArea.setPanel(scrollContent);
    scrollArea.setWidgetResizable(true);

    let mainLayout = form.create_vlayout();
    mainLayout.addWidget(scrollArea);
    dialog.setLayout(mainLayout);

    let accepted = dialog.exec();
    activePreview = null;

    if (accepted === true) {
        let campaign = {
            name:         txtName.text(),
            subject:      txtSubject.text(),
            sender_email: txtSenderEmail.text(),
            sender_name:  txtSenderName.text(),
            smtp_host:    txtSmtpHost.text(),
            smtp_port:    spinSmtpPort.value(),
            smtp_user:    txtSmtpUser.text(),
            smtp_pass:    txtSmtpPass.text(),
            smtp_tls:     chkSmtpTLS.isChecked(),
            template:     cmbTemplate.currentText(),
            lander:       cmbLander.currentText(),
            base_url:     txtBaseURL.text(),
            redirect_url: txtRedirectURL.text(),
            track_opens:  chkTrackOpens.isChecked(),
            track_clicks: chkTrackClicks.isChecked(),
            send_delay:   spinDelay.value()
        };

        if (!campaign.name || !campaign.smtp_host || !campaign.sender_email || !campaign.base_url) {
            ax.show_message("Error", "Required fields: Campaign Name, SMTP Host, Sender Email, Base URL");
            return;
        }

        ax.service_command("Phishing", "campaign_create", campaign);
    }
}

// ============================================================================
// Targets Dialog
// ============================================================================

function showTargetsDialog(campaignID, targets) {
    let dialog = form.create_ext_dialog("Targets - Campaign");
    dialog.setSize(700, 500);

    let mainLayout = form.create_vlayout();

    // Toolbar
    let toolbar = form.create_hlayout();
    let btnImport = form.create_button("Import CSV");
    let btnDelete = form.create_button("Delete Selected");
    toolbar.addWidget(btnImport);
    toolbar.addWidget(btnDelete);
    toolbar.addWidget(form.create_hspacer());

    let tgtToolbarPanel = form.create_panel();
    tgtToolbarPanel.setLayout(toolbar);
    mainLayout.addWidget(tgtToolbarPanel);

    // Table
    let tgtTable = form.create_table(["Email", "First Name", "Last Name", "Position", "Company"]);
    tgtTable.setSortingEnabled(true);
    tgtTable.setReadOnly(true);

    if (targets) {
        for (let i = 0; i < targets.length; i++) {
            let t = targets[i];
            tgtTable.addItem([t.email, t.first_name, t.last_name, t.position, t.company]);
        }
    }
    mainLayout.addWidget(tgtTable);

    dialog.setLayout(mainLayout);

    form.connect(btnImport, "clicked", function() {
        let csvDialog = form.create_dialog("Import Targets (CSV)");
        csvDialog.setSize(560, 450);

        let csvLayout = form.create_vlayout();
        csvLayout.addWidget(form.create_label("Paste CSV data or load a file. Columns: email, first_name, last_name, position, company"));
        csvLayout.addWidget(form.create_label("The first row must be column headers. Only 'email' is required."));
        let csvText = form.create_textmulti("email,first_name,last_name,position,company\njohn.doe@contoso.com,John,Doe,CFO,Contoso Ltd\njane.smith@contoso.com,Jane,Smith,IT Manager,Contoso Ltd\n");
        csvLayout.addWidget(csvText);

        let orLabel = form.create_label("Or load from file:");
        csvLayout.addWidget(orLabel);

        let btnFile = form.create_button("Load CSV File");
        csvLayout.addWidget(btnFile);

        form.connect(btnFile, "clicked", function() {
            let path = ax.prompt_open_file("Select CSV file", "CSV Files (*.csv);;All Files (*)");
            if (path) {
                let content = ax.file_read(path);
                if (content) {
                    csvText.setText(content);
                }
            }
        });

        csvDialog.setLayout(csvLayout);
        if (csvDialog.exec() === true) {
            let csv = csvText.text();
            if (csv && csv.trim().length > 0) {
                ax.service_command("Phishing", "targets_import", {
                    campaign_id: campaignID,
                    csv: csv
                });
            }
        }
    });

    form.connect(btnDelete, "clicked", function() {
        let rows = tgtTable.selectedRows();
        if (rows.length === 0) return;

        let ids = [];
        for (let i = 0; i < rows.length; i++) {
            if (targets && rows[i] < targets.length) {
                ids.push(targets[rows[i]].id);
            }
        }

        if (ids.length > 0 && ax.prompt_confirm("Delete Targets", "Delete " + ids.length + " selected target(s)?")) {
            ax.service_command("Phishing", "targets_delete", {
                campaign_id: campaignID,
                ids: ids
            });
        }
    });

    dialog.show();
}

// ============================================================================
// Results Dock
// ============================================================================

function createResultsDock() {
    resultsDock = form.create_ext_dock("phishing_results", "Phishing Results", "");

    let mainLayout = form.create_vlayout();

    // Filter bar
    let filterLayout = form.create_hlayout();
    filterLayout.addWidget(form.create_label("Campaign:"));
    campaignFilter = form.create_combo();
    campaignFilter.addItem("-- All --");
    filterLayout.addWidget(campaignFilter);

    let btnExport = form.create_button("Export CSV");
    let btnRefresh = form.create_button("Refresh");
    filterLayout.addWidget(form.create_hspacer());
    filterLayout.addWidget(btnExport);
    filterLayout.addWidget(btnRefresh);

    let filterPanel = form.create_panel();
    filterPanel.setLayout(filterLayout);
    mainLayout.addWidget(filterPanel);

    // Table
    resultsTable = form.create_table(["Campaign", "Email", "Name", "Status", "Sent", "Opened", "Clicked", "Submitted", "IP", "User Agent"]);
    resultsTable.setSortingEnabled(true);
    resultsTable.setReadOnly(true);
    mainLayout.addWidget(resultsTable);

    resultsDock.setLayout(mainLayout);
    resultsDock.setSize(1000, 400);
    resultsDock.show();

    // Signals
    form.connect(campaignFilter, "currentTextChanged", function(text) {
        refreshResultsTable();
    });

    form.connect(btnExport, "clicked", function() {
        let cid = getSelectedCampaignID();
        if (cid) {
            ax.service_command("Phishing", "results_export", {campaign_id: cid});
        } else {
            ax.show_message("Export", "Please select a specific campaign to export");
        }
    });

    form.connect(btnRefresh, "clicked", function() {
        loadAllResults();
    });

    form.connect(resultsTable, "cellDoubleClicked", function(row, col) {
        showResultDetail(row);
    });
}

function updateCampaignFilter() {
    if (!campaignFilter) return;

    let current = campaignFilter.currentText();
    campaignFilter.clear();
    campaignFilter.addItem("-- All --");

    if (campaignsData) {
        for (let i = 0; i < campaignsData.length; i++) {
            campaignFilter.addItem(campaignsData[i].name);
        }
    }

    // Restore selection
    for (let i = 0; i < campaignFilter.count; i++) {
        if (campaignFilter.itemText && campaignFilter.itemText(i) === current) {
            campaignFilter.setCurrentIndex(i);
            return;
        }
    }
}

function getSelectedCampaignID() {
    if (!campaignFilter) return null;
    let text = campaignFilter.currentText();
    if (text === "-- All --") return null;

    if (campaignsData) {
        for (let i = 0; i < campaignsData.length; i++) {
            if (campaignsData[i].name === text) {
                return campaignsData[i].id;
            }
        }
    }
    return null;
}

function refreshResultsTable() {
    if (!resultsTable) return;
    resultsTable.setRowCount(0);

    let filterCampaign = getSelectedCampaignID();

    for (let cid in allResults) {
        if (filterCampaign && cid !== filterCampaign) continue;

        let campaignName = getCampaignNameByID(cid);
        let results = allResults[cid];
        if (!results) continue;

        for (let i = 0; i < results.length; i++) {
            let r = results[i];
            let name = (r.first_name || "") + " " + (r.last_name || "");
            let sentAt = r.sent_at ? ax.format_time("HH:mm:ss", r.sent_at) : "";
            let openedAt = r.opened_at ? ax.format_time("HH:mm:ss", r.opened_at) : "";
            let clickedAt = r.clicked_at ? ax.format_time("HH:mm:ss", r.clicked_at) : "";
            let submitAt = r.submit_at ? ax.format_time("HH:mm:ss", r.submit_at) : "";

            resultsTable.addItem([
                campaignName,
                r.email || "",
                name.trim(),
                r.status || "",
                sentAt,
                openedAt,
                clickedAt,
                submitAt,
                r.remote_ip || "",
                r.user_agent || ""
            ]);
        }
    }
}

function getCampaignNameByID(id) {
    if (campaignsData) {
        for (let i = 0; i < campaignsData.length; i++) {
            if (campaignsData[i].id === id) return campaignsData[i].name;
        }
    }
    return id;
}

function showResultDetail(row) {
    // Collect result data from the table row for display
    let campaign = resultsTable.text(row, 0);
    let email = resultsTable.text(row, 1);
    let name = resultsTable.text(row, 2);
    let status = resultsTable.text(row, 3);
    let ip = resultsTable.text(row, 8);
    let ua = resultsTable.text(row, 9);

    let detail = "Campaign: " + campaign + "\n" +
                 "Email: " + email + "\n" +
                 "Name: " + name + "\n" +
                 "Status: " + status + "\n" +
                 "IP: " + ip + "\n" +
                 "User Agent: " + ua;

    ax.show_message("Result Detail", detail);
}

// ============================================================================
// Event Handling
// ============================================================================

function handleEvent(eventType, data) {
    if (!data || !data.campaign_id) return;

    let cid = data.campaign_id;
    let result = data.result;

    // Update local results cache
    if (result && allResults[cid]) {
        let found = false;
        for (let i = 0; i < allResults[cid].length; i++) {
            if (allResults[cid][i].id === result.id) {
                allResults[cid][i] = result;
                found = true;
                break;
            }
        }
        if (!found) {
            allResults[cid].push(result);
        }
    } else if (result && !allResults[cid]) {
        allResults[cid] = [result];
    }

    refreshResultsTable();

    // Also refresh campaign stats
    ax.service_command("Phishing", "campaign_list", {});
}

// ============================================================================
// Data Loading
// ============================================================================

function loadInitialData() {
    ax.service_command("Phishing", "campaign_list", {});
    ax.service_command("Phishing", "templates_list", {});
    ax.service_command("Phishing", "landers_list", {});
    loadAllResults();
}

function loadAllResults() {
    if (campaignsData) {
        for (let i = 0; i < campaignsData.length; i++) {
            ax.service_command("Phishing", "results_list", {campaign_id: campaignsData[i].id});
        }
    }
}
