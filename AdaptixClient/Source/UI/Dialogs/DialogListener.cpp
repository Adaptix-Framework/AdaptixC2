#include <UI/Dialogs/DialogListener.h>
#include <Client/Requestor.h>

DialogListener::DialogListener()
{
    this->createUI();

    connect(listenerTypeCombobox, &QComboBox::currentTextChanged, this, &DialogListener::changeConfig);
    connect(buttonLoad, &QPushButton::clicked, this, &DialogListener::onButtonLoad );
    connect(buttonSave, &QPushButton::clicked, this, &DialogListener::onButtonSave );
    connect(buttonCreate, &QPushButton::clicked, this, &DialogListener::onButtonCreate );
    connect(buttonCancel, &QPushButton::clicked, this, &DialogListener::onButtonCancel );
}

DialogListener::~DialogListener() = default;

void DialogListener::createUI()
{
    this->resize( 650, 650 );
    this->setWindowTitle( "Create Listener" );

    listenerNameLabel = new QLabel(this);
    listenerNameLabel->setText("Listener name:");

    inputListenerName = new QLineEdit(this);

    listenerTypeLabel = new QLabel(this);
    listenerTypeLabel->setText("Listener type: ");

    listenerTypeCombobox = new QComboBox(this);

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

    listenerConfigGroupbox = new QGroupBox( this );
    listenerConfigGroupbox->setTitle("Listener config");
    listenerConfigGroupbox->setLayout(stackGridLayout);

    buttonCreate = new QPushButton(this);
    buttonCreate->setText("Create");

    buttonCancel = new QPushButton(this);
    buttonCancel->setText("Cancel");

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->addItem(horizontalSpacer_2);
    hLayoutBottom->addWidget(buttonCreate);
    hLayoutBottom->addWidget(buttonCancel);
    hLayoutBottom->addItem(horizontalSpacer_3);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->addWidget( listenerNameLabel,      0, 0, 1, 1);
    mainGridLayout->addWidget( inputListenerName,      0, 1, 1, 1);
    mainGridLayout->addWidget( line_1,                 0, 2, 2, 1);
    mainGridLayout->addWidget( buttonLoad,             0, 3, 1, 1);
    mainGridLayout->addWidget( listenerTypeLabel,      1, 0, 1, 1);
    mainGridLayout->addWidget( listenerTypeCombobox,   1, 1, 1, 1);
    mainGridLayout->addWidget( buttonSave,             1, 3, 1, 1);
    mainGridLayout->addItem(   horizontalSpacer,       2, 0, 1, 4);
    mainGridLayout->addWidget( listenerConfigGroupbox, 3, 0, 1, 4);
    mainGridLayout->addLayout( hLayoutBottom,          4, 0, 1, 4);

    this->setLayout(mainGridLayout);

    int buttonWidth  = buttonCancel->width();
    buttonLoad->setFixedWidth(buttonWidth/2);
    buttonSave->setFixedWidth(buttonWidth/2);
    buttonCreate->setFixedWidth(buttonWidth);
    buttonCancel->setFixedWidth(buttonWidth);

    int buttonHeight = buttonCancel->height();
    buttonLoad->setFixedHeight(buttonHeight);
    buttonSave->setFixedHeight(buttonHeight);
    buttonCreate->setFixedHeight(buttonHeight);
    buttonCancel->setFixedHeight(buttonHeight);
}

void DialogListener::Start()
{
    this->exec();
}

void DialogListener::AddExListeners(const QMap<QString, WidgetBuilder*> &listeners)
{
    listenersUI = listeners;

    for (auto w : listenersUI.values()) {
        configStackWidget->addWidget( w->GetWidget() );
    }

    listenerTypeCombobox->clear();
    listenerTypeCombobox->addItems( listenersUI.keys() );
}

void DialogListener::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
}

void DialogListener::SetEditMode(const QString &name)
{
    this->setWindowTitle( "Edit Listener" );
    inputListenerName->setText(name);
    inputListenerName->setDisabled(true);
    listenerTypeCombobox->setDisabled(true);
    buttonCreate->setText("Edit");
    editMode = true;
}

void DialogListener::changeConfig(const QString &fn)
{
    if (listenersUI[fn]) {
        auto w = listenersUI[fn]->GetWidget();
        configStackWidget->setCurrentWidget(w);
    }
}

void DialogListener::onButtonCreate()
{
    auto configName= inputListenerName->text();
    auto configType= listenerTypeCombobox->currentText();
    auto configData = QString();
    if (listenersUI[configType])
        configData = listenersUI[configType]->CollectData();

    QString message = QString();
    bool result, ok = false;
    if ( editMode )
        result = HttpReqListenerEdit(configName, configType, configData, authProfile, &message, &ok);
    else
        result = HttpReqListenerStart(configName, configType, configData, authProfile, &message, &ok);

    if( !result ) {
        MessageError("JWT error");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }

    this->close();
}

void DialogListener::onButtonLoad()
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

    if ( !jsonObject.contains("type") || !jsonObject["type"].isString() ) {
        MessageError("Required parameter 'type' is missing");
        return;
    }
    if ( !jsonObject.contains("config") || !jsonObject["config"].isString() ) {
        MessageError("Required parameter 'config' is missing");
        return;
    }

    if( jsonObject.contains("name") && jsonObject["name"].isString())
        inputListenerName->setText( jsonObject["name"].toString() );

    QString configType = jsonObject["type"].toString();
    int typeIndex = listenerTypeCombobox->findText( configType );
    if(typeIndex == -1 || !listenersUI.contains(configType)) {
        MessageError("No such listener exists");
        return;
    }

    QString configData = jsonObject["config"].toString();
    listenerTypeCombobox->setCurrentIndex(typeIndex);
    listenersUI[configType]->FillData(configData);
}

void DialogListener::onButtonSave()
{
    auto configName= inputListenerName->text();
    auto configType= listenerTypeCombobox->currentText();
    auto configData = QString();
    if (listenersUI[configType])
        configData = listenersUI[configType]->CollectData();

    QJsonObject dataJson;
    dataJson["name"]   = configName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = configName + "_listener_config.json";
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

void DialogListener::onButtonCancel()
{
    this->close();
}