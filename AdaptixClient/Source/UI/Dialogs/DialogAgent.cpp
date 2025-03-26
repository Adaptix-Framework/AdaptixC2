#include <UI/Dialogs/DialogAgent.h>
#include <Client/Requestor.h>

DialogAgent::DialogAgent(const QString &listenerName, const QString &listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect(buttonLoad, &QPushButton::clicked, this, &DialogAgent::onButtonLoad );
    connect(buttonSave, &QPushButton::clicked, this, &DialogAgent::onButtonSave );
    connect( agentCombobox, &QComboBox::currentTextChanged, this, &DialogAgent::changeConfig);
    connect( generateButton, &QPushButton::clicked, this, &DialogAgent::onButtonGenerate );
    connect( closeButton, &QPushButton::clicked, this, &DialogAgent::onButtonClose );
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->resize( 450, 450 );
    this->setWindowTitle( "Generate Agent" );

    listenerLabel = new QLabel(this);
    listenerLabel->setText("Listener:");

    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel = new QLabel(this);
    agentLabel->setText("Agent: ");

    agentCombobox = new QComboBox(this);

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

    agentConfigGroupbox = new QGroupBox( this );
    agentConfigGroupbox->setTitle("Agent config");
    agentConfigGroupbox->setLayout(stackGridLayout);

    generateButton = new QPushButton(this);
    generateButton->setText("Generate");

    closeButton = new QPushButton(this);
    closeButton->setText("Close");

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
    mainGridLayout->addWidget( line_1,              0, 2, 2, 1);
    mainGridLayout->addWidget( buttonLoad,          0, 3, 1, 1);
    mainGridLayout->addWidget( agentLabel,          1, 0, 1, 1);
    mainGridLayout->addWidget( agentCombobox,       1, 1, 1, 1);
    mainGridLayout->addWidget( buttonSave,          1, 3, 1, 1);
    mainGridLayout->addItem(   horizontalSpacer,    2, 0, 1, 4);
    mainGridLayout->addWidget( agentConfigGroupbox, 3, 0, 1, 4);
    mainGridLayout->addLayout( hLayoutBottom,       4, 0, 1, 4);

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

void DialogAgent::AddExAgents(const QMap<QString, WidgetBuilder*> &agents)
{
    agentsUI = agents;

    for (auto w : agentsUI.values()) {
        configStackWidget->addWidget( w->GetWidget() );
    }

    agentCombobox->clear();
    agentCombobox->addItems( agentsUI.keys() );
}

void DialogAgent::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
}

void DialogAgent::changeConfig(const QString &fn)
{
    if (agentsUI.contains(fn) && agentsUI[fn]) {
        auto w = agentsUI[fn]->GetWidget();
        configStackWidget->setCurrentWidget(w);
    }
}

void DialogAgent::onButtonGenerate()
{
    auto agentName  = agentCombobox->currentText();
    auto configData = QString();
    if (agentsUI[agentName]) {
        configData = agentsUI[agentName]->CollectData();
    }

    QString message = QString();
    bool ok = false;
    bool result = HttpReqAgentGenerate( listenerName, listenerType, agentName, configData, authProfile, &message, &ok);
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
    if(typeIndex == -1 || !agentsUI.contains(agentType)) {
        MessageError("No such agent exists");
        return;
    }

    QString configData = jsonObject["config"].toString();
    agentCombobox->setCurrentIndex(typeIndex);
    agentsUI[agentType]->FillData(configData);
}

void DialogAgent::onButtonSave()
{
    QString agentName    = agentCombobox->currentText();
    QString configData   = "";
    if (agentsUI[agentName]) {
        configData = agentsUI[agentName]->CollectData();
    }

    QJsonObject dataJson;
    dataJson["listener_type"] = listenerType;
    dataJson["agent"]   = agentName;
    dataJson["config"] = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = agentName + "_agent_config.json";
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
