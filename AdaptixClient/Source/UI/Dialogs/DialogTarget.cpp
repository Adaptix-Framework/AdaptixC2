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
    this->resize(500, 250);
    this->setWindowTitle( "Add target" );
    this->setProperty("Main", "base");

    computerLabel = new QLabel("Computer:", this);
    computerInput = new QLineEdit(this);

    domainLabel = new QLabel("Domain:", this);
    domainInput = new QLineEdit(this);

    addressLabel = new QLabel("Address:", this);
    addressInput = new QLineEdit(this);

    aliveCheck = new QCheckBox("alive", this);

    osLabel = new QLabel("OS type:", this);
    osCombo = new QComboBox(this);
    osCombo->addItems(QStringList() << "unknown" << "windows" << "linux" << "macos");

    osDescLabel = new QLabel("OS description:", this);
    osDescInput = new QLineEdit(this);

    tagLabel  = new QLabel("Tag:", this);
    tagInput  = new QLineEdit(this);

    infoLabel  = new QLabel("Info:", this);
    infoInput = new QLineEdit(this);

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
    mainGridLayout->addWidget(computerLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget(computerInput, 0, 1, 1, 1);
    mainGridLayout->addWidget(domainLabel,   1, 0, 1, 1);
    mainGridLayout->addWidget(domainInput,   1, 1, 1, 1);
    mainGridLayout->addWidget(addressLabel,  2, 0, 1, 1);
    mainGridLayout->addWidget(addressInput,  2, 1, 1, 1);
    mainGridLayout->addWidget(aliveCheck,    3, 1, 1, 1);
    mainGridLayout->addWidget(osLabel,       4, 0, 1, 1);
    mainGridLayout->addWidget(osCombo,       4, 1, 1, 1);
    mainGridLayout->addWidget(osDescLabel,   5, 0, 1, 1);
    mainGridLayout->addWidget(osDescInput,   5, 1, 1, 1);
    mainGridLayout->addWidget(tagLabel,      6, 0, 1, 1);
    mainGridLayout->addWidget(tagInput,      6, 1, 1, 1);
    mainGridLayout->addWidget(infoLabel,     7, 0, 1, 1);
    mainGridLayout->addWidget(infoInput,     7, 1, 1, 1);
    mainGridLayout->addLayout(hLayoutBottom, 8, 0, 1, 2);

    int buttonWidth  = createButton->width();
    createButton->setFixedWidth(buttonWidth);
    cancelButton->setFixedWidth(buttonWidth);

    int buttonHeight = createButton->height();
    createButton->setFixedHeight(buttonHeight);
    cancelButton->setFixedHeight(buttonHeight);
}

void DialogTarget::StartDialog()
{
    this->valid   = false;
    this->message = "";
    this->exec();
}

void DialogTarget::SetEditmode(const TargetData &targetData)
{
    this->setWindowTitle( "Edit target" );
    this->targetId = targetData.TargetId;

    computerInput->setText(targetData.Computer);
    domainInput->setText(targetData.Domain);
    addressInput->setText(targetData.Address);
    aliveCheck->setChecked(targetData.Alive);
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
    data.Alive    = aliveCheck->isChecked();
    data.Os       = osCombo->currentIndex();
    data.OsDesc   = osDescInput->text();
    data.Tag      = tagInput->text();
    data.Info     = infoInput->text();

    this->valid = true;
    this->close();
}

void DialogTarget::onButtonCancel() { this->close(); }