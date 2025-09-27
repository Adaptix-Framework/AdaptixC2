/// Gopher TCP listener

function ListenerUI(mode_create)
{
    let labelHost = form.create_label("Host & port (Bind):");
    let comboHostBind = form.create_combo();
    comboHostBind.setEnabled(mode_create)
    comboHostBind.clear();
    let addrs = ax.interfaces();
    for (let item of addrs) { comboHostBind.addItem(item); }
    let spinPort = form.create_spin();
    spinPort.setRange(1, 65535);
    spinPort.setValue(4444);
    spinPort.setEnabled(mode_create)

    let labelCallback = form.create_label("Callback addresses:");
    let texteditCallback = form.create_textmulti();
    texteditCallback.setPlaceholder("192.168.1.1:4444\nserver2.com:5555");

    let labelTimeout = form.create_label("Timeout (sec):");
    let spinTimeout = form.create_spin();
    spinTimeout.setRange(1, 1000000);
    spinTimeout.setValue(10);

    let labelBanner = form.create_label("TCP banner:");
    let texteditBanner = form.create_textmulti("AdaptixC2 server\n");

    let labelAnswer = form.create_label("Error answer:");
    let texteditAnswer = form.create_textmulti("Connection error...\n");

    let labelEncryptKey = form.create_label("Encryption key:");
    let textlineEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textlineEncryptKey.setEnabled(mode_create)
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create)

    let checkMtls = form.create_check("Use mTLS");
    checkMtls.setEnabled(mode_create)

    let howButton = form.create_button("How generate?");

    let caCertSelector = form.create_selector_file();
    caCertSelector.setPlaceholder("CA cert");
    caCertSelector.setEnabled(false);

    let srvCertSelector = form.create_selector_file();
    srvCertSelector.setPlaceholder("Server cert");
    srvCertSelector.setEnabled(false);

    let srvKeySelector = form.create_selector_file();
    srvKeySelector.setPlaceholder("Server key");
    srvKeySelector.setEnabled(false);

    let clientCertSelector = form.create_selector_file();
    clientCertSelector.setPlaceholder("Client cert");
    clientCertSelector.setEnabled(false);

    let clientKeySelector = form.create_selector_file();
    clientKeySelector.setPlaceholder("Client key");
    clientKeySelector.setEnabled(false);

    let layout = form.create_gridlayout();
    layout.addWidget(labelHost,          0, 0, 1, 1);
    layout.addWidget(comboHostBind,      0, 1, 1, 2);
    layout.addWidget(spinPort,           0, 3, 1, 1);
    layout.addWidget(labelCallback,      1, 0, 1, 1);
    layout.addWidget(texteditCallback,   1, 1, 1, 3);
    layout.addWidget(labelTimeout,       2, 0, 1, 1);
    layout.addWidget(spinTimeout,        2, 1, 1, 3);
    layout.addWidget(labelBanner,        3, 0, 1, 1);
    layout.addWidget(texteditBanner,     3, 1, 1, 3);
    layout.addWidget(labelAnswer,        4, 0, 1, 1);
    layout.addWidget(texteditAnswer,     4, 1, 1, 3);
    layout.addWidget(labelEncryptKey,    5, 0, 1, 1);
    layout.addWidget(textlineEncryptKey, 5, 1, 1, 2);
    layout.addWidget(buttonEncryptKey,   5, 3, 1, 1);
    layout.addWidget(checkMtls,          6, 0, 1, 1);
    layout.addWidget(howButton,          6, 1, 1, 1);
    layout.addWidget(caCertSelector,     6, 2, 1, 2);
    layout.addWidget(srvKeySelector,     7, 0, 1, 2);
    layout.addWidget(srvCertSelector,    7, 2, 1, 2);
    layout.addWidget(clientKeySelector,  8, 0, 1, 2);
    layout.addWidget(clientCertSelector, 8, 2, 1, 2);

    form.connect(buttonEncryptKey, "clicked", function() { textlineEncryptKey.setText( ax.random_string(32, "hex") ); });

    form.connect(checkMtls, "stateChanged", function() {
        if(caCertSelector.getEnabled()) {
            caCertSelector.setEnabled(false);
            srvCertSelector.setEnabled(false);
            srvKeySelector.setEnabled(false);
            clientCertSelector.setEnabled(false);
            clientKeySelector.setEnabled(false);
        } else {
            caCertSelector.setEnabled(true);
            srvCertSelector.setEnabled(true);
            srvKeySelector.setEnabled(true);
            clientCertSelector.setEnabled(true);
            clientKeySelector.setEnabled(true);
        }
    });

    form.connect(howButton, "clicked", function() {
        let dialog = form.create_dialog("Generate mTLS certificates");

        let infoText = form.create_textmulti();
        infoText.setReadOnly(true);
        infoText.appendText("# CA cert");
        infoText.appendText("openssl genrsa -out ca.key 2048");
        infoText.appendText("openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj \"/CN=Test CA\"\n");
        infoText.appendText("# server cert and key");
        infoText.appendText("openssl genrsa -out server.key 2048");
        infoText.appendText("openssl req -new -key server.key -out server.csr -subj \"/CN=localhost\"");
        infoText.appendText("openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256\n");
        infoText.appendText("# client cert and key");
        infoText.appendText("openssl genrsa -out client.key 2048");
        infoText.appendText("openssl req -new -key client.key -out client.csr -subj \"/CN=client\"");
        infoText.appendText("openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 -sha256");

        let layout = form.create_vlayout();
        layout.addWidget(infoText);

        dialog.setLayout(layout);
        dialog.setSize(1100, 340);
        dialog.exec()
    });

    let container = form.create_container()
    container.put("host_bind", comboHostBind)
    container.put("port_bind", spinPort)
    container.put("callback_addresses", texteditCallback)
    container.put("timeout", spinTimeout)
    container.put("tcp_banner", texteditBanner)
    container.put("error_answer", texteditAnswer)
    container.put("encrypt_key", textlineEncryptKey);
    container.put("ssl", checkMtls)
    container.put("ca_cert", caCertSelector)
    container.put("server_cert", srvCertSelector)
    container.put("server_key", srvKeySelector)
    container.put("client_cert", clientCertSelector)
    container.put("client_key", clientKeySelector)

    let panel = form.create_panel()
    panel.setLayout(layout)

    return {
        ui_panel: panel,
        ui_container: container
    }
}
