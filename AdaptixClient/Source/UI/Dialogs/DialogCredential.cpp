#include <UI/Dialogs/DialogCredential.h>

DialogCredential::DialogCredential()
{
    this->createUI();

    connect(createButton, &QPushButton::clicked, this, &DialogCredential::onButtonCreate);
    connect(cancelButton, &QPushButton::clicked, this, &DialogCredential::onButtonCancel);
}

DialogCredential::~DialogCredential() = default;

void DialogCredential::createUI()
{
    this->resize(500, 350);
    this->setWindowTitle("Create Credential");
    this->setProperty("Main", "base");

    credGroup = new QGroupBox("Credential", this);
    credGrid = new QGridLayout(credGroup);
    credGrid->setContentsMargins(12, 12, 12, 12);
    credGrid->setSpacing(8);

    auto* usernameLabel = new QLabel("Username", credGroup);
    usernameInput = new QLineEdit(credGroup);
    usernameInput->setPlaceholderText("admin");

    auto* passwordLabel = new QLabel("Password", credGroup);
    passwordInput = new QLineEdit(credGroup);
    passwordInput->setPlaceholderText("Password or NTLM hash");

    auto* realmLabel = new QLabel("Realm", credGroup);
    realmInput = new QLineEdit(credGroup);
    realmInput->setPlaceholderText("CORP.LOCAL");

    auto* typeLabel = new QLabel("Type", credGroup);
    typeCombo = new QComboBox(credGroup);
    typeCombo->setEditable(true);
    typeCombo->addItems(QStringList() << "password" << "hash" << "rc4" << "aes128" << "aes256" << "token");
    typeCombo->setCurrentText("");

    credGrid->addWidget(usernameLabel, 0, 0);
    credGrid->addWidget(usernameInput, 0, 1);
    credGrid->addWidget(passwordLabel, 1, 0);
    credGrid->addWidget(passwordInput, 1, 1);
    credGrid->addWidget(realmLabel,    2, 0);
    credGrid->addWidget(realmInput,    2, 1);
    credGrid->addWidget(typeLabel,     3, 0);
    credGrid->addWidget(typeCombo,     3, 1);

    sourceGroup = new QGroupBox("Source", this);
    sourceGrid = new QGridLayout(sourceGroup);
    sourceGrid->setContentsMargins(12, 12, 12, 12);
    sourceGrid->setSpacing(8);

    auto* storageLabel = new QLabel("Storage", sourceGroup);
    storageCombo = new QComboBox(sourceGroup);
    storageCombo->setEditable(true);
    storageCombo->addItems(QStringList() << "browser" << "dpapi" << "database" << "sam" << "lsass" << "ntds" << "manual");
    storageCombo->setCurrentText("");

    auto* hostLabel = new QLabel("Host", sourceGroup);
    hostInput = new QLineEdit(sourceGroup);
    hostInput->setPlaceholderText("DC01");

    auto* tagLabel = new QLabel("Tag", sourceGroup);
    tagInput = new QLineEdit(sourceGroup);
    tagInput->setPlaceholderText("lateral, db...");

    sourceGrid->addWidget(storageLabel, 0, 0);
    sourceGrid->addWidget(storageCombo, 0, 1);
    sourceGrid->addWidget(hostLabel,    1, 0);
    sourceGrid->addWidget(hostInput,    1, 1);
    sourceGrid->addWidget(tagLabel,     2, 0);
    sourceGrid->addWidget(tagInput,     2, 1);

    createButton = new QPushButton("Create Credential", this);
    createButton->setDefault(true);
    cancelButton = new QPushButton("Cancel", this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(createButton);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(credGroup);
    mainLayout->addWidget(sourceGroup);
    mainLayout->addLayout(buttonLayout);

    this->setLayout(mainLayout);
}

void DialogCredential::StartDialog()
{
    this->valid = false;
    this->message = "";
    this->editMode = false;
    this->setWindowTitle("Create Credential");
    createButton->setText("Create Credential");
    this->exec();
}

void DialogCredential::SetEditmode(const CredentialData &credentialData)
{
    this->editMode = true;
    this->setWindowTitle("Edit Credential");
    createButton->setText("Update Credential");
    this->credsId = credentialData.CredId;

    usernameInput->setText(credentialData.Username);
    passwordInput->setText(credentialData.Password);
    realmInput->setText(credentialData.Realm);
    typeCombo->setCurrentText(credentialData.Type);
    storageCombo->setCurrentText(credentialData.Storage);
    hostInput->setText(credentialData.Host);
    tagInput->setText(credentialData.Tag);
}

bool DialogCredential::IsValid() const { return this->valid; }
QString DialogCredential::GetMessage() const { return this->message; }
CredentialData DialogCredential::GetCredData() const { return this->data; }

void DialogCredential::onButtonCreate()
{
    data = {};
    data.CredId   = this->credsId;
    data.Username = usernameInput->text();
    data.Password = passwordInput->text();
    data.Realm    = realmInput->text();
    data.Type     = typeCombo->currentText();
    data.Storage  = storageCombo->currentText();
    data.Host     = hostInput->text();
    data.Tag      = tagInput->text();

    if (data.Username.isEmpty() && data.Password.isEmpty()) {
        this->valid = false;
        this->message = "Username or Password must be set";
        return;
    }

    this->valid = true;
    this->close();
}

void DialogCredential::onButtonCancel() { this->close(); }
