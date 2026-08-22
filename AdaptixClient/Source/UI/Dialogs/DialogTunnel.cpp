#include <UI/Dialogs/DialogTunnel.h>
#include <Client/Requestor.h>
#include <Utils/FontManager.h>

DialogTunnel::DialogTunnel(qint64 agentId, const bool s4, const bool s5, const bool lpf, const bool rpf)
{
     this->createUI();
     this->AgentId = agentId;

     if (s5) { typeSegment->addItem("SOCKS5"); typeNames << "SOCKS5"; }
     if (s4) { typeSegment->addItem("SOCKS4"); typeNames << "SOCKS4"; }
     if (lpf) { typeSegment->addItem("LPF"); typeNames << "LPF"; }
     if (rpf) { typeSegment->addItem("RPF"); typeNames << "RPF"; }

     connect(typeSegment, &SegmentControl::currentIndexChanged, this, [this]() { changeType(typeSegment->currentIndex()); });
     connect(endpointSegment, &SegmentControl::currentIndexChanged, this, [this]() { changeType(typeSegment->currentIndex()); });
     connect(buttonCreate, &QPushButton::clicked, this, &DialogTunnel::onButtonCreate);
     connect(buttonCancel, &QPushButton::clicked, this, &DialogTunnel::onButtonCancel);
     connect(socks5UseAuth, &oclero::qlementine::Switch::toggled, this, &DialogTunnel::onSocks5AuthCheckChange);

     changeType(0);
}

DialogTunnel::~DialogTunnel() = default;

void DialogTunnel::createUI()
{
    this->resize(400, 350);
    this->setWindowTitle("Create Tunnel");
    this->setProperty("Main", "base");

    typeSegment = new SegmentControl(this);

    endpointSegment = new SegmentControl(this);
    endpointSegment->addItem("Server");
    endpointSegment->addItem("Client");

    segLayout = new QHBoxLayout();
    segLayout->setSpacing(8);
    segLayout->addWidget(typeSegment);
    segLayout->addWidget(endpointSegment);

    descLabel = new QLabel("Description", this);
    descInput = new QLineEdit(this);
    descInput->setPlaceholderText("Optional description...");

    configGroup = new QGroupBox(this);
    configGroup->setTitle("SOCKS5 Configuration");

    stackWidget = new QStackedWidget(this);

    configGrid = new QGridLayout(configGroup);
    configGrid->setContentsMargins(0, 0, 0, 0);
    configGrid->addWidget(stackWidget, 0, 0);

    // SOCKS5 page
    socks5Widget = new QWidget(this);
    auto* s5Grid = new QGridLayout(socks5Widget);
    s5Grid->setContentsMargins(12, 12, 12, 12);
    s5Grid->setSpacing(8);

    auto* s5AddrLabel = new QLabel("Listen address", socks5Widget);
    socks5AddrInput = new QLineEdit("0.0.0.0", socks5Widget);
    auto* s5PortLabel = new QLabel("Listen port", socks5Widget);
    socks5PortSpin = new QSpinBox(socks5Widget);
    socks5PortSpin->setRange(1, 65535);
    socks5PortSpin->setValue(1080);
    socks5UseAuth = new oclero::qlementine::Switch(socks5Widget);
    socks5UseAuth->setText("Enable username/password");
    auto* s5UserLabel = new QLabel("Username", socks5Widget);
    socks5UserInput = new QLineEdit(socks5Widget);
    socks5UserInput->setEnabled(false);
    auto* s5PassLabel = new QLabel("Password", socks5Widget);
    socks5PassInput = new QLineEdit(socks5Widget);
    socks5PassInput->setEnabled(false);

    s5Grid->addWidget(s5AddrLabel,     0, 0);
    s5Grid->addWidget(socks5AddrInput, 0, 1);
    s5Grid->addWidget(s5PortLabel,     1, 0);
    s5Grid->addWidget(socks5PortSpin,  1, 1);
    s5Grid->addWidget(socks5UseAuth,   2, 0, 1, 2);
    s5Grid->addWidget(s5UserLabel,     3, 0);
    s5Grid->addWidget(socks5UserInput, 3, 1);
    s5Grid->addWidget(s5PassLabel,     4, 0);
    s5Grid->addWidget(socks5PassInput, 4, 1);
    stackWidget->addWidget(socks5Widget);

    // SOCKS4 page
    socks4Widget = new QWidget(this);
    auto* s4Grid = new QGridLayout(socks4Widget);
    s4Grid->setContentsMargins(12, 12, 12, 12);
    s4Grid->setSpacing(8);

    auto* s4AddrLabel = new QLabel("Listen address", socks4Widget);
    socks4AddrInput = new QLineEdit("0.0.0.0", socks4Widget);
    auto* s4PortLabel = new QLabel("Listen port", socks4Widget);
    socks4PortSpin = new QSpinBox(socks4Widget);
    socks4PortSpin->setRange(1, 65535);
    socks4PortSpin->setValue(1080);

    s4Grid->addWidget(s4AddrLabel,     0, 0);
    s4Grid->addWidget(socks4AddrInput, 0, 1);
    s4Grid->addWidget(s4PortLabel,     1, 0);
    s4Grid->addWidget(socks4PortSpin,  1, 1);
    stackWidget->addWidget(socks4Widget);

    // LPF page
    lpfWidget = new QWidget(this);
    auto* lpfGrid = new QGridLayout(lpfWidget);
    lpfGrid->setContentsMargins(12, 12, 12, 12);
    lpfGrid->setSpacing(8);

    auto* lpfAddrLabel = new QLabel("Listen address", lpfWidget);
    lpfAddrInput = new QLineEdit("0.0.0.0", lpfWidget);
    auto* lpfPortLabel = new QLabel("Listen port", lpfWidget);
    lpfPortSpin = new QSpinBox(lpfWidget);
    lpfPortSpin->setRange(1, 65535);
    lpfPortSpin->setValue(8000);
    auto* lpfTAddrLabel = new QLabel("Target address", lpfWidget);
    lpfTargetAddrInput = new QLineEdit("127.0.0.1", lpfWidget);
    auto* lpfTPortLabel = new QLabel("Target port", lpfWidget);
    lpfTargetPortSpin = new QSpinBox(lpfWidget);
    lpfTargetPortSpin->setRange(1, 65535);
    lpfTargetPortSpin->setValue(8000);

    lpfGrid->addWidget(lpfAddrLabel,        0, 0);
    lpfGrid->addWidget(lpfAddrInput,        0, 1);
    lpfGrid->addWidget(lpfPortLabel,        1, 0);
    lpfGrid->addWidget(lpfPortSpin,         1, 1);
    lpfGrid->addWidget(lpfTAddrLabel,       2, 0);
    lpfGrid->addWidget(lpfTargetAddrInput,  2, 1);
    lpfGrid->addWidget(lpfTPortLabel,       3, 0);
    lpfGrid->addWidget(lpfTargetPortSpin,   3, 1);
    stackWidget->addWidget(lpfWidget);

    // RPF page
    rpfWidget = new QWidget(this);
    auto* rpfGrid = new QGridLayout(rpfWidget);
    rpfGrid->setContentsMargins(12, 12, 12, 12);
    rpfGrid->setSpacing(8);

    auto* rpfPortLabel = new QLabel("Listen port (agent)", rpfWidget);
    rpfPortSpin = new QSpinBox(rpfWidget);
    rpfPortSpin->setRange(1, 65535);
    rpfPortSpin->setValue(8000);
    auto* rpfTAddrLabel = new QLabel("Target address", rpfWidget);
    rpfTargetAddrInput = new QLineEdit("127.0.0.1", rpfWidget);
    auto* rpfTPortLabel = new QLabel("Target port", rpfWidget);
    rpfTargetPortSpin = new QSpinBox(rpfWidget);
    rpfTargetPortSpin->setRange(1, 65535);
    rpfTargetPortSpin->setValue(8000);
    rpfHintLabel = new QLabel(rpfWidget);
    rpfHintLabel->setWordWrap(true);
    rpfHintLabel->setObjectName(QStringLiteral("RpfHint"));
    rpfHintLabel->setStyleSheet(QStringLiteral("color: palette(placeholderText);"));

    rpfGrid->addWidget(rpfPortLabel,       0, 0);
    rpfGrid->addWidget(rpfPortSpin,        0, 1);
    rpfGrid->addWidget(rpfTAddrLabel,      1, 0);
    rpfGrid->addWidget(rpfTargetAddrInput, 1, 1);
    rpfGrid->addWidget(rpfTPortLabel,      2, 0);
    rpfGrid->addWidget(rpfTargetPortSpin,  2, 1);
    rpfGrid->addWidget(rpfHintLabel,       3, 0, 1, 2);
    stackWidget->addWidget(rpfWidget);

    buttonCreate = new QPushButton("Create Tunnel", this);
    buttonCreate->setDefault(true);
    buttonCancel = new QPushButton("Cancel", this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(buttonCancel);
    buttonLayout->addWidget(buttonCreate);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(segLayout);
    mainLayout->addWidget(descLabel);
    mainLayout->addWidget(descInput);
    mainLayout->addWidget(configGroup);
    mainLayout->addLayout(buttonLayout);

    this->setLayout(mainLayout);
}

void DialogTunnel::StartDialog()
{
    this->valid = false;
    this->message = "";
    this->exec();
}

bool DialogTunnel::IsValid() const { return this->valid; }

QString DialogTunnel::GetMessage() const { return this->message; }

QString DialogTunnel::GetTunnelType() const { return this->tunnelType; }

QString DialogTunnel::GetEndpoint() const
{
     return endpointSegment->currentIndex() == 0 ? "Teamserver" : "Client";
}

QByteArray DialogTunnel::GetTunnelData() const { return this->jsonData; }

void DialogTunnel::changeType(int index) const
{
    if (index < 0 || index >= typeNames.size()) return;
    QString type = typeNames[index];

    if (type == "SOCKS5") {
        stackWidget->setCurrentIndex(0);
        configGroup->setTitle("SOCKS5 Configuration");
        endpointSegment->setVisible(true);
    } else if (type == "SOCKS4") {
        stackWidget->setCurrentIndex(1);
        configGroup->setTitle("SOCKS4 Configuration");
        endpointSegment->setVisible(true);
    } else if (type == "LPF") {
        stackWidget->setCurrentIndex(2);
        configGroup->setTitle("LPF Configuration");
        endpointSegment->setVisible(true);
    } else if (type == "RPF") {
        stackWidget->setCurrentIndex(3);
        configGroup->setTitle("RPF Configuration");
        endpointSegment->setVisible(true);
    }
}

void DialogTunnel::onSocks5AuthCheckChange() const
{
    bool active = socks5UseAuth->isChecked();
    socks5UserInput->setEnabled(active);
    socks5PassInput->setEnabled(active);
}

void DialogTunnel::onButtonCreate()
{
    int idx = typeSegment->currentIndex();
    if (idx < 0 || idx >= typeNames.size()) return;
    QString type = typeNames[idx];

    QJsonObject dataJson;
    dataJson["agent_id"] = this->AgentId;
    dataJson["desc"]     = this->descInput->text();
    dataJson["listen"]   = endpointSegment->currentIndex() == 0;

    if (type == "SOCKS5") {
        QString l_host   = socks5AddrInput->text();
        int     l_port   = socks5PortSpin->value();
        bool    use_auth = socks5UseAuth->isChecked();
        QString username = socks5UserInput->text();
        QString password = socks5PassInput->text();

        if (l_host.isEmpty()) {
            this->valid = false; this->message = "Listen host must be set"; this->close(); return;
        }
        if (use_auth) {
            if (username.isEmpty()) { this->valid = false; this->message = "Username must be set"; this->close(); return; }
            if (password.isEmpty()) { this->valid = false; this->message = "Password must be set"; this->close(); return; }
        } else {
            username = ""; password = "";
        }

        this->tunnelType = "socks5";
        this->valid = true;
        dataJson["l_host"]   = l_host;
        dataJson["l_port"]   = l_port;
        dataJson["use_auth"] = use_auth;
        dataJson["username"] = username;
        dataJson["password"] = password;
    }
    else if (type == "SOCKS4") {
        QString l_host = socks4AddrInput->text();
        int     l_port = socks4PortSpin->value();

        if (l_host.isEmpty()) { this->valid = false; this->message = "Listen host must be set"; this->close(); return; }

        this->tunnelType = "socks4";
        this->valid = true;
        dataJson["l_host"] = l_host;
        dataJson["l_port"] = l_port;
    }
    else if (type == "LPF") {
        QString l_host = lpfAddrInput->text();
        int     l_port = lpfPortSpin->value();
        QString t_host = lpfTargetAddrInput->text();
        int     t_port = lpfTargetPortSpin->value();

        if (l_host.isEmpty()) { this->valid = false; this->message = "Listen host must be set"; this->close(); return; }
        if (t_host.isEmpty()) { this->valid = false; this->message = "Target host must be set"; this->close(); return; }

        this->tunnelType = "lportfwd";
        this->valid = true;
        dataJson["l_host"] = l_host;
        dataJson["l_port"] = l_port;
        dataJson["t_host"] = t_host;
        dataJson["t_port"] = t_port;
    }
    else if (type == "RPF") {
        int     port   = rpfPortSpin->value();
        QString t_host = rpfTargetAddrInput->text();
        int     t_port = rpfTargetPortSpin->value();

        if (t_host.isEmpty()) { this->valid = false; this->message = "Target host must be set"; this->close(); return; }

        this->tunnelType = "rportfwd";
        this->valid = true;
        dataJson["port"]   = port;
        dataJson["t_host"] = t_host;
        dataJson["t_port"] = t_port;
    }
    else {
        this->message = "Unknown tunnel type";
        this->valid = false;
        this->close();
        return;
    }
    this->jsonData = QJsonDocument(dataJson).toJson();
    this->close();
}

void DialogTunnel::onButtonCancel() { this->close(); }