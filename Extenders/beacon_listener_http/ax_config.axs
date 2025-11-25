/// Beacon HTTP listener

function ListenerUI(mode_create)
{
    // MAIN SETTING
    let labelHost = form.create_label("Host & port (Bind):");
    let comboHostBind = form.create_combo();
    comboHostBind.setEnabled(mode_create)
    comboHostBind.clear();
    let addrs = ax.interfaces();
    for (let item of addrs) { comboHostBind.addItem(item); }
    let spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(443);
    spinPortBind.setEnabled(mode_create)

    let labelCallback = form.create_label("Callback addresses:");
    let textCallback = form.create_textmulti();
    textCallback.setPlaceholder("192.168.1.1:443\nserver2.com:8080");

    let labelMethod = form.create_label("Method:");
    let comboMethod = form.create_combo();
    comboMethod.addItems(["POST", "GET"]);
    comboMethod.setEnabled(mode_create)

    let labelUri = form.create_label("URI:");
    let textlineUri = form.create_textline();
    textlineUri.setPlaceholder("/uri.php");

    let labelUserAgent = form.create_label("User-Agent:");
    let textlineUserAgent = form.create_textline("Mozilla/5.0 (Windows NT 6.2; rv:20.0) Gecko/20121202 Firefox/20.0");

    let labelHB = form.create_label("Heartbeat Header:");
    let textlineHB = form.create_textline("X-Beacon-Id");

    let labelEncryptKey = form.create_label("Encryption key:");
    let textlineEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textlineEncryptKey.setEnabled(mode_create)
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create)

    let certSelector = form.create_selector_file();
    certSelector.setPlaceholder("SSL certificate");
    let keySelector = form.create_selector_file();
    keySelector.setPlaceholder("SSL key");
    let layout_group = form.create_vlayout();
    layout_group.addWidget(certSelector);
    layout_group.addWidget(keySelector);
    let panel_group = form.create_panel();
    panel_group.setLayout(layout_group);
    let ssl_group = form.create_groupbox("Use SSL (HTTPS)", true)
    ssl_group.setPanel(panel_group);
    ssl_group.setChecked(false);

    form.connect(buttonEncryptKey, "clicked", function() { textlineEncryptKey.setText( ax.random_string(32, "hex") ); });

    let layoutMain = form.create_gridlayout();
    layoutMain.addWidget(labelHost, 0, 0, 1, 1);
    layoutMain.addWidget(comboHostBind, 0, 1, 1, 1);
    layoutMain.addWidget(spinPortBind, 0, 2, 1, 1);
    layoutMain.addWidget(labelCallback, 1, 0, 1, 1);
    layoutMain.addWidget(textCallback, 1, 1, 1, 2);
    layoutMain.addWidget(labelMethod, 2, 0, 1, 1);
    layoutMain.addWidget(comboMethod, 2, 1, 1, 2);
    layoutMain.addWidget(labelUri, 3, 0, 1, 1);
    layoutMain.addWidget(textlineUri, 3, 1, 1, 2);
    layoutMain.addWidget(labelUserAgent, 4, 0, 1, 1 );
    layoutMain.addWidget(textlineUserAgent, 4, 1, 1, 2);
    layoutMain.addWidget(labelHB, 5, 0, 1, 1);
    layoutMain.addWidget(textlineHB, 5, 1, 1, 2);
    layoutMain.addWidget(labelEncryptKey, 6, 0, 1, 1);
    layoutMain.addWidget(textlineEncryptKey, 6, 1, 1, 1);
    layoutMain.addWidget(buttonEncryptKey, 6, 2, 1, 1);
    layoutMain.addWidget(ssl_group, 7, 0, 1, 3);

    let panelMain = form.create_panel();
    panelMain.setLayout(layoutMain);

    // HTTP HEADERS
    let checkTrust = form.create_check("Trust X-Forwarded-For");

    let labelHostHeader = form.create_label("Host Header:");
    let textlineHostHeader = form.create_textline();

    let labelRequestHeaders = form.create_label("Request Headers:");
    let textRequestHeaders = form.create_textmulti();

    let labelServerHeaders = form.create_label("Server Headers:");
    let textServerHeaders = form.create_textmulti();
    textServerHeaders.setEnabled(mode_create)

    let layoutHeaders = form.create_gridlayout();
    layoutHeaders.addWidget(checkTrust, 0, 0, 1, 2);
    layoutHeaders.addWidget(labelHostHeader, 1, 0, 1, 1);
    layoutHeaders.addWidget(textlineHostHeader, 1, 1, 1, 1);
    layoutHeaders.addWidget(labelRequestHeaders, 2, 0, 1, 1);
    layoutHeaders.addWidget(textRequestHeaders, 2, 1, 1, 1);
    layoutHeaders.addWidget(labelServerHeaders, 3, 0, 1, 1);
    layoutHeaders.addWidget(textServerHeaders, 3, 1, 1, 1);

    let panelHeaders = form.create_panel();
    panelHeaders.setLayout(layoutHeaders);

    // HTTP HEADERS
    let textError = form.create_textmulti("<!DOCTYPE html>\n<html>\n<head>\n<title>ERROR 404 - Nothing Found</title>\n</head>\n<body>\n<h1 class=\"cover-heading\">ERROR 404 - PAGE NOT FOUND</h1>\n</div>\n</div>\n</div>\n</body>\n</html>");

    let layoutError = form.create_gridlayout();
    layoutError.addWidget(textError, 0, 0, 1, 1);

    let panelError = form.create_panel();
    panelError.setLayout(layoutError);

    // PAYLOAD
    let textPayload = form.create_textmulti("{\"status\": \"ok\", \"data\": \"<<<PAYLOAD_DATA>>>\", \"metrics\": \"sync\"}");

    let layoutPayload = form.create_gridlayout();
    layoutPayload.addWidget(textPayload, 0, 0, 1, 1);

    let panelPayload = form.create_panel();
    panelPayload.setLayout(layoutPayload);

    //
    let tabs = form.create_tabs();
    tabs.addTab(panelMain, "Main settings");
    tabs.addTab(panelHeaders, "HTTP Headers");
    tabs.addTab(panelError, "Page Error");
    tabs.addTab(panelPayload, "Page Payload");

    let layout = form.create_hlayout();
    layout.addWidget(tabs);

    let container = form.create_container();
    container.put("host_bind", comboHostBind);
    container.put("port_bind", spinPortBind);
    container.put("callback_addresses", textCallback);
    container.put("http_method", comboMethod);
    container.put("uri", textlineUri);
    container.put("user_agent", textlineUserAgent);
    container.put("hb_header", textlineHB);
    container.put("encrypt_key", textlineEncryptKey);
    container.put("ssl", ssl_group);
    container.put("ssl_cert", certSelector);
    container.put("ssl_key", keySelector);
    container.put("x-forwarded-for", checkTrust);
    container.put("host_header", textlineHostHeader);
    container.put("request_headers", textRequestHeaders);
    container.put("server_headers", textServerHeaders);
    container.put("page-error", textError);
    container.put("page-payload", textPayload);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 650,
        ui_width: 650
    }
}