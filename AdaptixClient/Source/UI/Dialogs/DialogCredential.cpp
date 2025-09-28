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
    this->resize(500, 300);
    this->setWindowTitle( "Add credentials" );
    this->setProperty("Main", "base");

    usernameLabel = new QLabel("Username:", this);
    usernameInput = new QLineEdit(this);

    passwordLabel = new QLabel("Password:", this);
    passwordInput = new QLineEdit(this);

    realmLabel = new QLabel("Realm:", this);
    realmInput = new QLineEdit(this);

    typeLabel = new QLabel("Type:", this);
    typeCombo = new QComboBox(this);
    typeCombo->setEditable(true);
    typeCombo->addItems(QStringList() << "password" << "hash" << "rc4" << "aes128" << "aes256" << "token");
    typeCombo->setCurrentText("");

    tagLabel = new QLabel("Tag:", this);
    tagInput = new QLineEdit(this);

    storageLabel = new QLabel("Storage:", this);
    storageCombo = new QComboBox(this);
    storageCombo->setEditable(true);
    storageCombo->addItems(QStringList() << "browser" << "dpapi" << "database" << "sam" << "lsass" << "ntds" << "manual");
    storageCombo->setCurrentText("");

    hostLabel = new QLabel("Host:", this);
    hostInput = new QLineEdit(this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    createButton = new QPushButton("Save", this);
    createButton->setProperty("ButtonStyle", "dialog");
    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setProperty("ButtonStyle", "dialog");

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->addItem(spacer_1);
    hLayoutBottom->addWidget(createButton);
    hLayoutBottom->addWidget(cancelButton);
    hLayoutBottom->addItem(spacer_2);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(4, 4,  4, 4 );
    mainGridLayout->addWidget(usernameLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget(usernameInput, 0, 1, 1, 1);
    mainGridLayout->addWidget(passwordLabel, 1, 0, 1, 1);
    mainGridLayout->addWidget(passwordInput, 1, 1, 1, 1);
    mainGridLayout->addWidget(realmLabel,    2, 0, 1, 1);
    mainGridLayout->addWidget(realmInput,    2, 1, 1, 1);
    mainGridLayout->addWidget(typeLabel,     3, 0, 1, 1);
    mainGridLayout->addWidget(typeCombo,     3, 1, 1, 1);
    mainGridLayout->addWidget(tagLabel,      4, 0, 1, 1);
    mainGridLayout->addWidget(tagInput,      4, 1, 1, 1);
    mainGridLayout->addWidget(storageLabel,  5, 0, 1, 1);
    mainGridLayout->addWidget(storageCombo,  5, 1, 1, 1);
    mainGridLayout->addWidget(hostLabel,     6, 0, 1, 1);
    mainGridLayout->addWidget(hostInput,     6, 1, 1, 1);
    mainGridLayout->addLayout(hLayoutBottom, 7, 0, 1, 2);

    int buttonWidth  = createButton->width();
    createButton->setFixedWidth(buttonWidth);
    cancelButton->setFixedWidth(buttonWidth);

    int buttonHeight = createButton->height();
    createButton->setFixedHeight(buttonHeight);
    cancelButton->setFixedHeight(buttonHeight);
}

void DialogCredential::StartDialog()
{
    this->valid = false;
    this->message = "";
    this->exec();
}

void DialogCredential::SetEditmode(const CredentialData &credentialData)
{
    this->setWindowTitle( "Edit credentials" );
    this->credsId = credentialData.CredId;

    this->usernameInput->setText(credentialData.Username);
    this->passwordInput->setText(credentialData.Password);
    this->realmInput->setText(credentialData.Realm);
    this->typeCombo->setCurrentText(credentialData.Type);
    this->tagInput->setText(credentialData.Tag);
    this->storageCombo->setCurrentText(credentialData.Storage);
    this->hostInput->setText(credentialData.Host);
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
    data.Tag      = tagInput->text();
    data.Storage  = storageCombo->currentText();
    data.Host     = hostInput->text();

    this->valid = true;
    this->close();
}

void DialogCredential::onButtonCancel() { this->close(); }