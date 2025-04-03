#include <UI/Dialogs/DialogAgent.h>
#include <Client/Requestor.h>
#include <UI/Widgets/AdaptixWidget.h>

DialogAgent::DialogAgent(const QString &listenerName, const QString &listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect( buttonLoad,     &QPushButton::clicked,          this, &DialogAgent::onButtonLoad );
    connect( buttonSave,     &QPushButton::clicked,          this, &DialogAgent::onButtonSave );
    connect( agentCombobox,  &QComboBox::currentTextChanged, this, &DialogAgent::changeConfig) ;
    connect( osCombobox,     &QComboBox::currentTextChanged, this, &DialogAgent::changeOs) ;
    connect( generateButton, &QPushButton::clicked,          this, &DialogAgent::onButtonGenerate );
    connect( closeButton,    &QPushButton::clicked,          this, &DialogAgent::onButtonClose );
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->resize( 450, 450 );
    this->setWindowTitle( "Generate Agent" );

    listenerLabel = new QLabel("Listener:", this);
    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel    = new QLabel("Agent: ", this);
    agentCombobox = new QComboBox(this);

    osLabel    = new QLabel("OS: ", this);
    osCombobox = new QComboBox(this);

    buttonLoad = new QPushButton(QIcon(":/icons/unarchive"), "", this);
    buttonLoad->setIconSize( QSize( 25,25 ));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/archive"), "", this);
    buttonSave->setIconSize( QSize( 25,25 ));
    buttonSave->setToolTip("Save profile to file");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(20);

    configStackWidget = new QStackedWidget(this );

    stackGridLayout = new QGridLayout(this );
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    agentConfigGroupbox = new QGroupBox( "Agent config", this );
    agentConfigGroupbox->setLayout(stackGridLayout);

    generateButton = new QPushButton( "Generate", this);
    closeButton    = new QPushButton( "Close", this);

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->addItem(horizontalSpacer_2);
    hLayoutBottom->addWidget(generateButton);
    hLayoutBottom->addWidget(closeButton);
    hLayoutBottom->addItem(horizontalSpacer_3);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->addWidget( listenerLabel,       0, 0, 1, 1);
    mainGridLayout->addWidget( listenerInput,       0, 1, 1, 1);
    mainGridLayout->addWidget( line_1,              0, 2, 3, 1);
    mainGridLayout->addWidget( buttonLoad,          0, 3, 1, 1);
    mainGridLayout->addWidget( agentLabel,          1, 0, 1, 1);
    mainGridLayout->addWidget( agentCombobox,       1, 1, 1, 1);
    mainGridLayout->addWidget( buttonSave,          1, 3, 1, 1);
    mainGridLayout->addWidget( osLabel,             2, 0, 1, 1);
    mainGridLayout->addWidget( osCombobox,          2, 1, 1, 1);
    mainGridLayout->addItem(   horizontalSpacer,    3, 0, 1, 4);
    mainGridLayout->addWidget( agentConfigGroupbox, 4, 0, 1, 4);
    mainGridLayout->addLayout( hLayoutBottom,       5, 0, 1, 4);

    this->setLayout(mainGridLayout);

    int buttonWidth = generateButton->width();
    buttonLoad->setFixedWidth(buttonWidth/2);
    buttonSave->setFixedWidth(buttonWidth/2);
    closeButton->setFixedWidth(buttonWidth);
    generateButton->setFixedWidth(buttonWidth);

    int buttonHeight = generateButton->height();
    buttonLoad->setFixedHeight(buttonHeight);
    buttonSave->setFixedHeight(buttonHeight);
    closeButton->setFixedHeight(buttonHeight);
    generateButton->setFixedHeight(buttonHeight);
}

void DialogAgent::Start()
{
    this->exec();
}

void DialogAgent::AddExAgents(const QVector<RegAgentConfig> &regAgents)
{
    agentCombobox->clear();
    osCombobox->clear();
    agentsOs.clear();

    this->regAgents = regAgents;

    QSet<QString> agents;
    for (auto regAgent : this->regAgents ) {
        agents.insert(regAgent.agentName);

        agentsOs[regAgent.agentName].insert(regAgent.operatingSystem);

        configStackWidget->addWidget(regAgent.builder->GetWidget());
    }

    for (QString v : agents)
        agentCombobox->addItem(v);
}

void DialogAgent::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
}

void DialogAgent::changeConfig(const QString &agentName)
{
    if (this->agentsOs.contains(agentName)) {
        osCombobox->clear();
        for (auto os : this->agentsOs[agentName])
            osCombobox->addItem(os);
    }
}

void DialogAgent::changeOs(const QString &os)
{
    QString agentName = agentCombobox->currentText();
    for (auto regAgent : this->regAgents ) {
        if (regAgent.agentName == agentName && regAgent.operatingSystem == os && regAgent.builder) {
            auto w = regAgent.builder->GetWidget();
            configStackWidget->setCurrentWidget(w);
            break;
        }
    }
}

void DialogAgent::onButtonGenerate()
{
    QString agentName = agentCombobox->currentText();
    QString operatingSystem = osCombobox->currentText();
    QString configData = "";

    for (auto regAgent : this->regAgents ) {
        if (regAgent.agentName == agentName && regAgent.operatingSystem == operatingSystem && regAgent.builder) {
            configData = regAgent.builder->CollectData();
            break;
        }
    }

    QString message = QString();
    bool ok = false;
    bool result = HttpReqAgentGenerate( listenerName, listenerType, agentName, operatingSystem, configData, authProfile, &message, &ok);
    if( !result ){
        MessageError("JWT error");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }

    QStringList parts = message.split(":");
    if (parts.size() != 2) {
        MessageError("The response format is not supported");
        return;
    }

    QString filename = QString( QByteArray::fromBase64(parts[0].toUtf8()));
    QByteArray content = QByteArray::fromBase64(parts[1].toUtf8());

    QString filePath = QFileDialog::getSaveFileName( nullptr, "Save File", filename, "All Files (*.*)" );
    if ( filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageError("Failed to open file for writing");
        return;
    }

    file.write( content );
    file.close();

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Save agent");
    inputDialog.setLabelText("File saved to:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(filePath);
    inputDialog.adjustSize();
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();

    this->close();
}

void DialogAgent::onButtonLoad()
{
    QString filePath = QFileDialog::getOpenFileName( nullptr, "Select file", QDir::homePath(), "JSON files (*.json)" );
    if ( filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray fileContent = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(fileContent, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        MessageError("Error JSON parse");
        return;
    }
    QJsonObject jsonObject = document.object();

    if ( !jsonObject.contains("listener_type") || !jsonObject["listener_type"].isString() ) {
        MessageError("Required parameter 'listener_type' is missing");
        return;
    }
    if ( !jsonObject.contains("agent") || !jsonObject["agent"].isString() ) {
        MessageError("Required parameter 'agent' is missing");
        return;
    }
    if ( !jsonObject.contains("operating_system") || !jsonObject["operating_system"].isString() ) {
        MessageError("Required parameter 'operating_system' is missing");
        return;
    }
    if ( !jsonObject.contains("config") || !jsonObject["config"].isString() ) {
        MessageError("Required parameter 'config' is missing");
        return;
    }

    if(listenerType != jsonObject["listener_type"].toString()) {
        MessageError("Listener type mismatch");
        return;
    }

    QString agentType = jsonObject["agent"].toString();

    int typeIndex = agentCombobox->findText( agentType );
    if ( typeIndex == -1 || !this->agentsOs.contains(agentType)) {
        MessageError("No such agent exists");
        return;
    }
    agentCombobox->setCurrentIndex(typeIndex);

    this->changeConfig(agentType);

    QString operatingSystem = jsonObject["operating_system"].toString();
    typeIndex = osCombobox->findText( operatingSystem );

    QStringList items;
    for (auto os : this->agentsOs[agentType])
        items.push_back(os);

    if(typeIndex == -1 || !items.contains(operatingSystem)) {
        MessageError("No such agent exists");
        return;
    }
    agentCombobox->setCurrentIndex(typeIndex);

    QString configData = jsonObject["config"].toString();
    for (auto regAgent : this->regAgents ) {
        if (regAgent.agentName == agentType && regAgent.operatingSystem == operatingSystem && regAgent.builder) {
            regAgent.builder->FillData(configData);
            break;
        }
    }
}

void DialogAgent::onButtonSave()
{
    QString agentName       = agentCombobox->currentText();
    QString operatingSystem = osCombobox->currentText();

    QString configData = "";
    for (auto regAgent : this->regAgents ) {
        if (regAgent.agentName == agentName && regAgent.operatingSystem == operatingSystem && regAgent.builder) {
            configData = regAgent.builder->CollectData();
            break;
        }
    }

    QJsonObject dataJson;
    dataJson["listener_type"]    = listenerType;
    dataJson["agent"]            = agentName;
    dataJson["operating_system"] = operatingSystem;
    dataJson["config"]           = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = QString("%1_%2_config.json").arg(agentName).arg(operatingSystem) ;
    QString filePath = QFileDialog::getSaveFileName( nullptr, "Save File", tmpFilename, "JSON files (*.json)" );
    if ( filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageError("Failed to open file for writing");
        return;
    }

    file.write( fileContent );
    file.close();

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Save config");
    inputDialog.setLabelText("File saved to:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(filePath);
    inputDialog.adjustSize();
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void DialogAgent::onButtonClose()
{
    this->close();
}
