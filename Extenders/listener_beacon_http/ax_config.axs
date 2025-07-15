/// Beacon HTTP listener

function ListenerUI(mode_create)
{
    // MAIN SETTING
    var labelHost = form.create_label("Host & port (Bind):");
    var textlineHostBind = form.create_textline("0.0.0.0");
    textlineHostBind.setEnabled(mode_create)
    var spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(443);
    spinPortBind.setEnabled(mode_create)

    var labelCallback = form.create_label("Callback addresses:");
    var textCallback = form.create_textmulti();
    textCallback.setPlaceholder("192.168.1.1:443\nserver2.com:8080");

    var labelMethod = form.create_label("Method:");
    var comboMethod = form.create_combo();
    comboMethod.addItems(["POST", "GET"]);
    comboMethod.setEnabled(mode_create)

    var labelUri = form.create_label("URI:");
    var textlineUri = form.create_textline();
    textlineUri.setPlaceholder("/uri.php");

    var labelUserAgent = form.create_label("User-Agent:");
    var textlineUserAgent = form.create_textline("Mozilla/5.0 (Windows NT 6.2; rv:20.0) Gecko/20121202 Firefox/20.0");

    var labelHB = form.create_label("Heartbeat Header:");
    var textlineHB = form.create_textline("X-Beacon-Id");

    var checkSsl = form.create_check("Use SSL (HTTPS)");
    checkSsl.setEnabled(mode_create)

    var certSelector = form.create_file_selector();
    certSelector.setPlaceholder("SSL certificate");
    var keySelector = form.create_file_selector();
    keySelector.setPlaceholder("SSL key");

    var layoutMain = form.create_gridlayout();
    layoutMain.addWidget(labelHost, 0, 0, 1, 1);
    layoutMain.addWidget(textlineHostBind, 0, 1, 1, 1);
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
    layoutMain.addWidget(checkSsl, 6, 0, 1, 3);
    layoutMain.addWidget(certSelector, 7, 0, 1, 3);
    layoutMain.addWidget(keySelector, 8, 0, 1, 3);

    var panelMain = form.create_panel();
    panelMain.setLayout(layoutMain);

    // HTTP HEADERS
    var checkTrust = form.create_check("Trust X-Forwarded-For");

    var labelHostHeader = form.create_label("Host Header:");
    var textlineHostHeader = form.create_textline();

    var labelRequestHeaders = form.create_label("Request Headers:");
    var textRequestHeaders = form.create_textmulti();

    var labelServerHeaders = form.create_label("Server Headers:");
    var textServerHeaders = form.create_textmulti();
    textServerHeaders.setEnabled(mode_create)

    var layoutHeaders = form.create_gridlayout();
    layoutHeaders.addWidget(checkTrust, 0, 0, 1, 2);
    layoutHeaders.addWidget(labelHostHeader, 1, 0, 1, 1);
    layoutHeaders.addWidget(textlineHostHeader, 1, 1, 1, 1);
    layoutHeaders.addWidget(labelRequestHeaders, 2, 0, 1, 1);
    layoutHeaders.addWidget(textRequestHeaders, 2, 1, 1, 1);
    layoutHeaders.addWidget(labelServerHeaders, 3, 0, 1, 1);
    layoutHeaders.addWidget(textServerHeaders, 3, 1, 1, 1);

    var panelHeaders = form.create_panel();
    panelHeaders.setLayout(layoutHeaders);

    // HTTP HEADERS
    var textError = form.create_textmulti("<!DOCTYPE html>\n<html>\n<head>\n<title>ERROR 404 - Nothing Found</title>\n</head>\n<body>\n<h1 class=\"cover-heading\">ERROR 404 - PAGE NOT FOUND</h1>\n</div>\n</div>\n</div>\n</body>\n</html>");

    var layoutError = form.create_gridlayout();
    layoutError.addWidget(textError, 0, 0, 1, 1);

    var panelError = form.create_panel();
    panelError.setLayout(layoutError);

    // PAYLOAD
    var textPayload = form.create_textmulti("{\"status\": \"ok\", \"data\": \"<<<PAYLOAD_DATA>>>\", \"metrics\": \"sync\"}");

    var layoutPayload = form.create_gridlayout();
    layoutPayload.addWidget(textPayload, 0, 0, 1, 1);

    var panelPayload = form.create_panel();
    panelPayload.setLayout(layoutPayload);

    //
    var tabs = form.create_tabs();
    tabs.addTab(panelMain, "Main settings");
    tabs.addTab(panelHeaders, "HTTP Headers");
    tabs.addTab(panelError, "Page Error");
    tabs.addTab(panelPayload, "Page Payload");

    var layout = form.create_hlayout();
    layout.addWidget(tabs);

    var container = form.create_container();
    container.put("host_bind", textlineHostBind);
    container.put("port_bind", spinPortBind);
    container.put("callback_addresses", textCallback);
    container.put("http_method", comboMethod);
    container.put("uri", textlineUri);
    container.put("user_agent", textlineUserAgent);
    container.put("hb_header", textlineHB);
    container.put("ssl", checkSsl);
    container.put("ssl_cert", certSelector);
    container.put("ssl_key", keySelector);
    container.put("x-forwarded-for", checkTrust);
    container.put("host_header", textlineHostHeader);
    container.put("request_headers", textRequestHeaders);
    container.put("server_headers", textServerHeaders);
    container.put("page-error", textError);
    container.put("page-payload", textPayload);

    certSelector.setEnabled(false);
    keySelector.setEnabled(false);

    form.connect(checkSsl, "stateChanged", function() {
        if(certSelector.getEnabled()) {
            certSelector.setEnabled(false);
            keySelector.setEnabled(false);
        } else {
            certSelector.setEnabled(true);
            keySelector.setEnabled(true);
        }
    });

    var panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container
    }
}