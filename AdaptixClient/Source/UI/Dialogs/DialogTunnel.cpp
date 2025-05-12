#include <UI/Dialogs/DialogTunnel.h>
#include <Client/Requestor.h>

DialogTunnel::DialogTunnel()
{
     this->createUI();
     connect(tunnelTypeCombo, &QComboBox::currentTextChanged, this, &DialogTunnel::changeType);
     connect(buttonCreate,    &QPushButton::clicked,          this, &DialogTunnel::onButtonCreate);
     connect(buttonCancel,    &QPushButton::clicked,          this, &DialogTunnel::onButtonCancel);
     connect(socks5UseAuth,   &QCheckBox::stateChanged,       this, &DialogTunnel::onSocks5AuthCheckChange );
}

DialogTunnel::~DialogTunnel() = default;

void DialogTunnel::createUI()
{
     this->resize(400, 350);
     this->setWindowTitle( "Create Tunnel" );

     tunnelTypeLabel = new QLabel("Tunnel type:", this);
     tunnelTypeCombo = new QComboBox(this);

     tunnelEndpointLabel = new QLabel("Tunnel endpoint:", this);
     tunnelEndpointCombo = new QComboBox(this);
     tunnelEndpointCombo->addItem("Teamserver");
     tunnelEndpointCombo->addItem("Client");

     tunnelDescLabel = new QLabel("Description: ",this);
     tunnelDescInput = new QLineEdit(this);

     tunnelStackWidget = new QStackedWidget(this );

     stackGridLayout = new QGridLayout(this );
     stackGridLayout->setHorizontalSpacing(0);
     stackGridLayout->setContentsMargins(0, 0, 0, 0 );
     stackGridLayout->addWidget(tunnelStackWidget, 0, 0, 1, 1 );

     tunnelConfigGroupbox = new QGroupBox(this);
     tunnelConfigGroupbox->setTitle("Settings");
     tunnelConfigGroupbox->setLayout(stackGridLayout);

     buttonCreate = new QPushButton(this);
     buttonCreate->setText("Create");

     buttonCancel = new QPushButton(this);
     buttonCancel->setText("Cancel");

     horizontalSpacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
     horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

     hLayoutBottom = new QHBoxLayout();
     hLayoutBottom->addItem(horizontalSpacer_1);
     hLayoutBottom->addWidget(buttonCreate);
     hLayoutBottom->addWidget(buttonCancel);
     hLayoutBottom->addItem(horizontalSpacer_2);

     mainGridLayout = new QGridLayout( this );
     mainGridLayout->addWidget( tunnelTypeLabel,      0, 0, 1, 1);
     mainGridLayout->addWidget( tunnelTypeCombo,      0, 1, 1, 1);
     mainGridLayout->addWidget( tunnelEndpointLabel,  1, 0, 1, 1);
     mainGridLayout->addWidget( tunnelEndpointCombo,  1, 1, 1, 1);
     mainGridLayout->addWidget( tunnelDescLabel,      2, 0, 1, 1);
     mainGridLayout->addWidget( tunnelDescInput,      2, 1, 1, 1);
     mainGridLayout->addWidget( tunnelConfigGroupbox, 3, 0, 1, 2);
     mainGridLayout->addLayout( hLayoutBottom,        4, 0, 1, 2);

     this->setLayout(mainGridLayout);

     int buttonWidth  = buttonCancel->width();
     buttonCreate->setFixedWidth(buttonWidth);
     buttonCancel->setFixedWidth(buttonWidth);

     int buttonHeight = buttonCancel->height();
     buttonCreate->setFixedHeight(buttonHeight);
     buttonCancel->setFixedHeight(buttonHeight);

     /// Socks5
     socks5Widget = new QWidget(this);
     socks5LocalAddrLabel = new QLabel("Listen:", socks5Widget);
     socks5LocalAddrInput = new QLineEdit("0.0.0.0", socks5Widget);
     socks5LocalPortSpin  = new QSpinBox(socks5Widget);
     socks5LocalPortSpin->setMinimum(1);
     socks5LocalPortSpin->setMaximum(65535);
     socks5LocalPortSpin->setValue(1080);
     socks5UseAuth       = new QCheckBox("Use authentication", socks5Widget);
     socks5AuthUserLabel = new QLabel("Username:", socks5Widget);
     socks5AuthUserInput = new QLineEdit(socks5Widget);
     socks5AuthUserInput->setEnabled(false);
     socks5AuthPassLabel = new QLabel("Password:", socks5Widget);
     socks5AuthPassInput = new QLineEdit(socks5Widget);
     socks5AuthPassInput->setEnabled(false);

     socks5GridLayout = new QGridLayout(socks5Widget);
     socks5GridLayout->addWidget(socks5LocalAddrLabel, 0, 0, 1, 1);
     socks5GridLayout->addWidget(socks5LocalAddrInput, 0, 1, 1, 1);
     socks5GridLayout->addWidget(socks5LocalPortSpin,  0, 2, 1, 1);
     socks5GridLayout->addWidget(socks5UseAuth,        1, 1, 1, 2);
     socks5GridLayout->addWidget(socks5AuthUserLabel,  2, 0, 1, 1);
     socks5GridLayout->addWidget(socks5AuthUserInput,  2, 1, 1, 2);
     socks5GridLayout->addWidget(socks5AuthPassLabel,  3, 0, 1, 1);
     socks5GridLayout->addWidget(socks5AuthPassInput,  3, 1, 1, 2);
     tunnelStackWidget->addWidget(socks5Widget);

     /// Socks4
     socks4Widget = new QWidget(this);
     socks4LocalAddrLabel = new QLabel("Listen:", socks4Widget);
     socks4LocalAddrInput = new QLineEdit("0.0.0.0", socks4Widget);
     socks4LocalPortSpin  = new QSpinBox(socks4Widget);
     socks4LocalPortSpin->setMinimum(1);
     socks4LocalPortSpin->setMaximum(65535);
     socks4LocalPortSpin->setValue(1080);

     socks4GridLayout = new QGridLayout(socks4Widget);
     socks4GridLayout->addWidget(socks4LocalAddrLabel, 0, 0, 1, 1);
     socks4GridLayout->addWidget(socks4LocalAddrInput, 0, 1, 1, 1);
     socks4GridLayout->addWidget(socks4LocalPortSpin,  0, 2, 1, 1);
     tunnelStackWidget->addWidget(socks4Widget);

     /// LPF
     lpfWidget = new QWidget(this);
     lpfLocalAddrLabel = new QLabel("Listen:", lpfWidget);
     lpfLocalAddrInput = new QLineEdit("0.0.0.0", lpfWidget);
     lpfLocalPortSpin  = new QSpinBox(lpfWidget);
     lpfLocalPortSpin->setMinimum(1);
     lpfLocalPortSpin->setMaximum(65535);
     lpfLocalPortSpin->setValue(8000);
     lpfTargetAddrLabel = new QLabel("Target:", lpfWidget);
     lpfTargetAddrInput = new QLineEdit("127.0.0.1", lpfWidget);
     lpfTargetPortSpin  = new QSpinBox(lpfWidget);
     lpfTargetPortSpin->setMinimum(1);
     lpfTargetPortSpin->setMaximum(65535);
     lpfTargetPortSpin->setValue(8000);

     lpfGridLayout = new QGridLayout(lpfWidget);
     lpfGridLayout->addWidget(lpfLocalAddrLabel,  0, 0, 1, 1);
     lpfGridLayout->addWidget(lpfLocalAddrInput,  0, 1, 1, 1);
     lpfGridLayout->addWidget(lpfLocalPortSpin,   0, 2, 1, 1);
     lpfGridLayout->addWidget(lpfTargetAddrLabel, 1, 0, 1, 1);
     lpfGridLayout->addWidget(lpfTargetAddrInput, 1, 1, 1, 1);
     lpfGridLayout->addWidget(lpfTargetPortSpin,  1, 2, 1, 1);
     tunnelStackWidget->addWidget(lpfWidget);

     // RPF
     rpfWidget = new QWidget(this);
     rpfPortLabel = new QLabel("Port:", rpfWidget);
     rpfPortSpin  = new QSpinBox(rpfWidget);
     rpfPortSpin->setMinimum(1);
     rpfPortSpin->setMaximum(65535);
     rpfPortSpin->setValue(8000);
     rpfTargetAddrLabel = new QLabel("Target:", rpfWidget);
     rpfTargetAddrInput = new QLineEdit("127.0.0.1", rpfWidget);
     rpfTargetPortSpin  = new QSpinBox(rpfWidget);
     rpfTargetPortSpin->setMinimum(1);
     rpfTargetPortSpin->setMaximum(65535);
     rpfTargetPortSpin->setValue(8000);

     rpfGridLayout = new QGridLayout(rpfWidget);
     rpfGridLayout->addWidget(rpfPortLabel,       0, 0, 1, 1);
     rpfGridLayout->addWidget(rpfPortSpin,        0, 1, 1, 2);
     rpfGridLayout->addWidget(rpfTargetAddrLabel, 1, 0, 1, 1);
     rpfGridLayout->addWidget(rpfTargetAddrInput, 1, 1, 1, 1);
     rpfGridLayout->addWidget(rpfTargetPortSpin,  1, 2, 1, 1);
     tunnelStackWidget->addWidget(rpfWidget);
}

void DialogTunnel::StartDialog()
{
     this->valid = false;
     this->message = "";
     this->exec();
}

bool DialogTunnel::IsValid() const
{
     return this->valid;
}

QString DialogTunnel::GetMessage() const
{
     return this->message;
}

QString DialogTunnel::GetTunnelType() const
{
     return this->tunnelType;
}

QString DialogTunnel::GetEndpoint() const
{
     return this->tunnelEndpointCombo->currentText();
}

QByteArray DialogTunnel::GetTunnelData() const
{
     return this->jsonData;
}

void DialogTunnel::SetSettings(const QString &agentId, const bool s5, const bool s4, const bool lpf, const bool rpf)
{
     tunnelTypeCombo->clear();
     this->AgentId = agentId;

     if (s5)  tunnelTypeCombo->addItem("Socks5");
     if (s4)  tunnelTypeCombo->addItem("Socks4");
     if (lpf) tunnelTypeCombo->addItem("Local port forwarding");
     if (rpf) tunnelTypeCombo->addItem("Reverse port forwarding");
}

void DialogTunnel::changeType(const QString &type) const
{
     tunnelEndpointCombo->clear();
     tunnelEndpointCombo->addItem("Teamserver");
     if (type == "Socks5") {
          tunnelStackWidget->setCurrentIndex(0);
          tunnelEndpointCombo->addItem("Client");
     }
     else if (type == "Socks4") {
          tunnelStackWidget->setCurrentIndex(1);
          tunnelEndpointCombo->addItem("Client");
     }
     else if (type == "Local port forwarding") {
          tunnelStackWidget->setCurrentIndex(2);
          tunnelEndpointCombo->addItem("Client");
     }
     else if (type == "Reverse port forwarding") {
          tunnelStackWidget->setCurrentIndex(3);
     }
}

void DialogTunnel::onSocks5AuthCheckChange() const
{
     bool active = socks5UseAuth->isChecked();
     socks5AuthUserInput->setEnabled(active);
     socks5AuthPassInput->setEnabled(active);
}

void DialogTunnel::onButtonCreate()
{
     QString type = tunnelTypeCombo->currentText();

     QJsonObject dataJson;
     dataJson["agent_id"] = this->AgentId;
     dataJson["desc"]     = this->tunnelDescInput->text();
     dataJson["listen"]   = this->tunnelEndpointCombo->currentText() == "Teamserver";

     if (type == "Socks5") {
          QString l_host   = this->socks5LocalAddrInput->text();
          int     l_port   = this->socks5LocalPortSpin->value();
          bool    use_auth = this->socks5UseAuth->isChecked();
          QString username = this->socks5AuthUserInput->text();
          QString password = this->socks5AuthPassInput->text();

          if (l_host.isEmpty()) {
               this->valid   = false;
               this->message = "Listen host must be set";
               this->close();
               return;
          }
          if (use_auth) {
               if (username.isEmpty()) {
                    this->valid   = false;
                    this->message = "Username host must be set";
                    this->close();
                    return;
               }
               if (password.isEmpty()) {
                    this->valid   = false;
                    this->message = "Password host must be set";
                    this->close();
                    return;
               }
          }
          else {
               username = "";
               password = "";
          }

          this->tunnelType = "socks5";
          this->valid = true;
          dataJson["l_host"]   = l_host;
          dataJson["l_port"]   = l_port;
          dataJson["use_auth"] = use_auth;
          dataJson["username"] = username;
          dataJson["password"] = password;

     }
     else if (type == "Socks4") {
          QString l_host = this->socks4LocalAddrInput->text();
          int     l_port = this->socks4LocalPortSpin->value();

          if (l_host.isEmpty()) {
               this->valid   = false;
               this->message = "Listen host must be set";
               this->close();
               return;
          }

          this->tunnelType = "socks4";
          this->valid = true;
          dataJson["l_host"] = l_host;
          dataJson["l_port"] = l_port;

     }
     else if (type == "Local port forwarding") {
          QString l_host = this->lpfLocalAddrInput->text();
          int     l_port = this->lpfLocalPortSpin->value();
          QString t_host = this->lpfTargetAddrInput->text();
          int     t_port = this->lpfTargetPortSpin->value();

          if (l_host.isEmpty()) {
               this->valid   = false;
               this->message = "Listen host must be set";
               this->close();
               return;
          }
          if (t_host.isEmpty()) {
               this->valid   = false;
               this->message = "Target host must be set";
               this->close();
               return;
          }

          this->tunnelType = "lportfwd";
          this->valid = true;
          dataJson["l_host"] = l_host;
          dataJson["l_port"] = l_port;
          dataJson["t_host"] = t_host;
          dataJson["t_port"] = t_port;

     }
     else if (type == "Reverse port forwarding") {
          int     port   = this->rpfPortSpin->value();
          QString t_host = this->rpfTargetAddrInput->text();
          int     t_port = this->rpfTargetPortSpin->value();

          if (t_host.isEmpty()) {
               this->valid   = false;
               this->message = "Target host must be set";
               this->close();
               return;
          }

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

void DialogTunnel::onButtonCancel()
{
     this->close();
}