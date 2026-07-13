#include <UI/Dialogs/DialogTarget.h>

DialogTarget::DialogTarget()
{
    this->createUI();

    connect(createButton, &QPushButton::clicked, this, &DialogTarget::onButtonCreate);
    connect(cancelButton, &QPushButton::clicked, this, &DialogTarget::onButtonCancel);
}

DialogTarget::~DialogTarget() = default;

void DialogTarget::createUI()
{
    this->resize(500, 350);
    this->setWindowTitle("Create Target");
    this->setProperty("Main", "base");

    hostGroup = new QGroupBox("Host", this);
    hostGrid = new QGridLayout(hostGroup);
    hostGrid->setContentsMargins(12, 12, 12, 12);
    hostGrid->setSpacing(8);

    auto* computerLabel = new QLabel("Computer", hostGroup);
    computerInput = new QLineEdit(hostGroup);
    computerInput->setPlaceholderText("DC01");

    auto* domainLabel = new QLabel("Domain", hostGroup);
    domainInput = new QLineEdit(hostGroup);
    domainInput->setPlaceholderText("CORP.LOCAL");

    auto* addressLabel = new QLabel("Address", hostGroup);
    addressInput = new QLineEdit(hostGroup);
    addressInput->setPlaceholderText("10.0.0.1");

    auto* aliveLabel = new QLabel("Alive", hostGroup);
    aliveSwitch = new oclero::qlementine::Switch(hostGroup);

    hostGrid->addWidget(computerLabel, 0, 0);
    hostGrid->addWidget(computerInput, 0, 1);
    hostGrid->addWidget(domainLabel,   1, 0);
    hostGrid->addWidget(domainInput,   1, 1);
    hostGrid->addWidget(addressLabel,  2, 0);
    hostGrid->addWidget(addressInput,  2, 1);
    hostGrid->addWidget(aliveLabel,    3, 0);
    hostGrid->addWidget(aliveSwitch,   3, 1);

    systemGroup = new QGroupBox("System", this);
    systemGrid = new QGridLayout(systemGroup);
    systemGrid->setContentsMargins(12, 12, 12, 12);
    systemGrid->setSpacing(8);

    auto* osLabel = new QLabel("OS Type", systemGroup);
    osCombo = new QComboBox(systemGroup);
    osCombo->addItems(QStringList() << "unknown" << "windows" << "linux" << "macos");

    auto* osDescLabel = new QLabel("Description", systemGroup);
    osDescInput = new QLineEdit(systemGroup);
    osDescInput->setPlaceholderText("Windows Server 2022");

    auto* tagLabel = new QLabel("Tag", systemGroup);
    tagInput = new QLineEdit(systemGroup);
    tagInput->setPlaceholderText("dc, fileserver...");

    auto* infoLabel = new QLabel("Info", systemGroup);
    infoInput = new QLineEdit(systemGroup);
    infoInput->setPlaceholderText("Domain controller");

    systemGrid->addWidget(osLabel,     0, 0);
    systemGrid->addWidget(osCombo,     0, 1);
    systemGrid->addWidget(osDescLabel, 1, 0);
    systemGrid->addWidget(osDescInput, 1, 1);
    systemGrid->addWidget(tagLabel,    2, 0);
    systemGrid->addWidget(tagInput,    2, 1);
    systemGrid->addWidget(infoLabel,   3, 0);
    systemGrid->addWidget(infoInput,   3, 1);

    createButton = new QPushButton("Create Target", this);
    createButton->setDefault(true);
    cancelButton = new QPushButton("Cancel", this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(createButton);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(hostGroup);
    mainLayout->addWidget(systemGroup);
    mainLayout->addLayout(buttonLayout);

    this->setLayout(mainLayout);
}

void DialogTarget::StartDialog()
{
    this->valid = false;
    this->message = "";
    this->editMode = false;
    this->setWindowTitle("Create Target");
    createButton->setText("Create Target");
    this->exec();
}

void DialogTarget::SetEditmode(const TargetData &targetData)
{
    this->editMode = true;
    this->setWindowTitle("Edit Target");
    createButton->setText("Update Target");
    this->targetId = targetData.TargetId;

    computerInput->setText(targetData.Computer);
    domainInput->setText(targetData.Domain);
    addressInput->setText(targetData.Address);
    aliveSwitch->setChecked(targetData.Alive);
    osCombo->setCurrentIndex(targetData.Os);
    osDescInput->setText(targetData.OsDesc);
    tagInput->setText(targetData.Tag);
    infoInput->setText(targetData.Info);
}

bool DialogTarget::IsValid() const { return this->valid; }
QString DialogTarget::GetMessage() const { return this->message; }
TargetData DialogTarget::GetTargetData() const { return this->data; }

void DialogTarget::onButtonCreate()
{
    data = {};
    data.TargetId = this->targetId;
    data.Computer = computerInput->text();
    data.Domain   = domainInput->text();
    data.Address  = addressInput->text();
    data.Alive    = aliveSwitch->isChecked();
    data.Os       = osCombo->currentIndex();
    data.OsDesc   = osDescInput->text();
    data.Tag      = tagInput->text();
    data.Info     = infoInput->text();

    if (data.Computer.isEmpty() && data.Address.isEmpty()) {
        this->valid = false;
        this->message = "Computer or Address must be set";
        return;
    }

    this->valid = true;
    this->close();
}

void DialogTarget::onButtonCancel() { this->close(); }
