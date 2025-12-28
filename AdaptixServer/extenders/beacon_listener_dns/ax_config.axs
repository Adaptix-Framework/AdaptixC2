/// Beacon DNS listener

function ListenerUI(mode_create)
{
    let spacer1 = form.create_vspacer();

    let labelHost = form.create_label("Host & Port (Bind):");
    let comboHostBind = form.create_combo();
    comboHostBind.setEnabled(mode_create);
    comboHostBind.clear();
    let addrs = ax.interfaces();
    for (let item of addrs) { comboHostBind.addItem(item); }
    let spinPortBind = form.create_spin();
    spinPortBind.setRange(1, 65535);
    spinPortBind.setValue(53);
    spinPortBind.setEnabled(mode_create);

    let labelDomain = form.create_label("Authoritative Domain(s):");
    let textDomain = form.create_textline("");
    textDomain.setPlaceholder("Comma-separated: ns1.c2.com,ns2.backup.com");

    let labelPktSize = form.create_label("Max Payload (bytes):");
    let spinPktSize = form.create_spin();
    spinPktSize.setRange(512, 65535);
    spinPktSize.setValue(4096);

    let labelTTL = form.create_label("DNS TTL (seconds):");
    let spinTTL = form.create_spin();
    spinTTL.setRange(1, 3600);
    spinTTL.setValue(5);

    let labelEncryptKey = form.create_label("Encryption Key:");
    let textEncryptKey = form.create_textline(ax.random_string(32, "hex"));
    textEncryptKey.setEnabled(mode_create);
    let buttonEncryptKey = form.create_button("Generate");
    buttonEncryptKey.setEnabled(mode_create);
    form.connect(buttonEncryptKey, "clicked", function() { textEncryptKey.setText(ax.random_string(32, "hex")); });

    let spacer2 = form.create_vspacer();

    let layout = form.create_gridlayout();
    layout.addWidget(spacer1,          0, 0, 1, 3);
    layout.addWidget(labelHost,        1, 0, 1, 1);
    layout.addWidget(comboHostBind,    1, 1, 1, 1);
    layout.addWidget(spinPortBind,     1, 2, 1, 1);
    layout.addWidget(labelDomain,      2, 0, 1, 1);
    layout.addWidget(textDomain,       2, 1, 1, 2);
    layout.addWidget(labelPktSize,     3, 0, 1, 1);
    layout.addWidget(spinPktSize,      3, 1, 1, 2);
    layout.addWidget(labelTTL,         4, 0, 1, 1);
    layout.addWidget(spinTTL,          4, 1, 1, 2);
    layout.addWidget(labelEncryptKey,  5, 0, 1, 1);
    layout.addWidget(textEncryptKey,   5, 1, 1, 1);
    layout.addWidget(buttonEncryptKey, 5, 2, 1, 1);
    layout.addWidget(spacer2,          6, 0, 1, 3);

    let container = form.create_container();
    container.put("host_bind",   comboHostBind);
    container.put("port_bind",   spinPortBind);
    container.put("domain",      textDomain);
    container.put("pkt_size",    spinPktSize);
    container.put("ttl",         spinTTL);
    container.put("encrypt_key", textEncryptKey);

    let panel = form.create_panel();
    panel.setLayout(layout);

    return {
        ui_panel: panel,
        ui_container: container,
        ui_height: 400,
        ui_width: 500
    }
}

function GetConfig()
{
    var config = {
        "name": "Beacon DNS",
        "type": "listener",
        "author": "Adaptix",
        "version": "1.0",
        "description": "Direct DNS tunneling listener. Agent connects directly to this DNS server.",
        "options": [
            {
                "name": "host_bind",
                "description": "Local interface to bind for DNS server",
                "type": "string",
                "default": "0.0.0.0",
                "required": true
            },
            {
                "name": "port_bind",
                "description": "DNS port (usually 53)",
                "type": "int",
                "default": "53",
                "required": true
            },
            {
                "name": "domain",
                "description": "Authoritative Domain(s) (comma-separated for failover)",
                "type": "string",
                "default": "",
                "required": true
            },
            {
                "name": "pkt_size",
                "description": "Max DNS Payload Size (bytes)",
                "type": "int",
                "default": "4096",
                "required": true
            },
            {
                "name": "ttl",
                "description": "DNS TTL (seconds)",
                "type": "int",
                "default": "5",
                "required": true
            },
            {
                "name": "encrypt_key",
                "description": "Encryption Key (32 hex chars or any string)",
                "type": "string",
                "default": "random_hex_32",
                "required": true
            }
        ]
    };
    return config;
}
