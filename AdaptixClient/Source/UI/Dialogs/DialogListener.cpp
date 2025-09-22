#include <UI/Dialogs/DialogListener.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>

DialogListener::DialogListener(QWidget *parent) : QDialog(parent)
{
    this->createUI();

    connect(listenerCombobox,     &QComboBox::currentTextChanged, this, &DialogListener::changeConfig);
    connect(listenerTypeCombobox, &QComboBox::currentTextChanged, this, &DialogListener::changeType);
    connect(buttonLoad,   &QPushButton::clicked, this, &DialogListener::onButtonLoad );
    connect(buttonSave,   &QPushButton::clicked, this, &DialogListener::onButtonSave );
    connect(buttonCreate, &QPushButton::clicked, this, &DialogListener::onButtonCreate );
    connect(buttonCancel, &QPushButton::clicked, this, &DialogListener::onButtonCancel );
}

DialogListener::~DialogListener() = default;

void DialogListener::createUI()
{
    this->resize(650, 650);
    this->setWindowTitle("Create Listener");
    this->setProperty("Main", "base");

    listenerNameLabel = new QLabel(this);
    listenerNameLabel->setText("Listener name:");

    inputListenerName = new QLineEdit(this);

    listenerLabel = new QLabel(this);
    listenerLabel->setText("Listener: ");
    listenerCombobox = new QComboBox(this);

    listenerTypeLabel = new QLabel(this);
    listenerTypeLabel->setText("Listener type: ");
    listenerTypeCombobox = new QComboBox(this);

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setIconSize( QSize( 25,25 ));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
    buttonSave->setIconSize( QSize( 25,25 ));
    buttonSave->setToolTip("Save profile to file");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(20);

    configStackWidget = new QStackedWidget(this);

    stackGridLayout = new QGridLayout(this);
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    listenerConfigGroupbox = new QGroupBox(this);
    listenerConfigGroupbox->setTitle("Listener config");
    listenerConfigGroupbox->setLayout(stackGridLayout);

    buttonCreate = new QPushButton(this);
    buttonCreate->setProperty("ButtonStyle", "dialog");
    buttonCreate->setText("Create");

    buttonCancel = new QPushButton(this);
    buttonCancel->setProperty("ButtonStyle", "dialog");
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
    mainGridLayout->addWidget( line_1,                 0, 2, 3, 1);
    mainGridLayout->addWidget( buttonLoad,             0, 3, 1, 1);
    mainGridLayout->addWidget( listenerTypeLabel,      1, 0, 1, 1);
    mainGridLayout->addWidget( listenerTypeCombobox,   1, 1, 1, 1);
    mainGridLayout->addWidget( buttonSave,             1, 3, 1, 1);
    mainGridLayout->addWidget( listenerLabel,          2, 0, 1, 1);
    mainGridLayout->addWidget( listenerCombobox,       2, 1, 1, 1);
    mainGridLayout->addItem(   horizontalSpacer,       3, 0, 1, 4);
    mainGridLayout->addWidget( listenerConfigGroupbox, 4, 0, 1, 4);
    mainGridLayout->addLayout( hLayoutBottom,          5, 0, 1, 4);

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

void DialogListener::Start() { this->exec(); }

void DialogListener::AddExListeners(const QList<RegListenerConfig> &listeners, const QMap<QString, QWidget*> &widgets, const QMap<QString, AxContainerWrapper*> &containers)
{
    listenerCombobox->clear();
    listenerTypeCombobox->clear();

    this->listeners  = listeners;
    this->widgets    = widgets;
    this->containers = containers;

    QSet<QString> listenersSet;

    for (auto listener : listeners) {
        widgets[listener.name]->setParent(nullptr);
        widgets[listener.name]->setParent(this);
        containers[listener.name]->setParent(nullptr);
        containers[listener.name]->setParent(this);

        configStackWidget->addWidget(widgets[listener.name]);
        listenerCombobox->addItem(listener.name);
        QString type = listener.type + " (" + listener.protocol + ")";
        listenersSet.insert(type);
    }
    listenerTypeCombobox->addItem("any");
    listenerTypeCombobox->addItems(QList<QString>(listenersSet.begin(), listenersSet.end()));
}

void DialogListener::SetProfile(const AuthProfile &profile) { this->authProfile = profile; }

void DialogListener::SetEditMode(const QString &name)
{
    this->setWindowTitle( "Edit Listener" );
    inputListenerName->setText(name);
    inputListenerName->setDisabled(true);
    listenerCombobox->setDisabled(true);
    listenerTypeCombobox->setDisabled(true);
    buttonCreate->setText("Edit");
    editMode = true;
}

void DialogListener::changeConfig(const QString &fn)
{
    if (widgets.contains(fn))
        configStackWidget->setCurrentWidget(widgets[fn]);
}

void DialogListener::changeType(const QString &type)
{
    listenerCombobox->clear();
    for (auto listener : listeners) {
        QString listenerType = listener.type + " (" + listener.protocol + ")";
        if (listenerType == type || type == "any")
            listenerCombobox->addItem(listener.name);
    }
}

void DialogListener::onButtonCreate()
{
    auto configName= inputListenerName->text();
    auto configType= listenerCombobox->currentText();
    auto configData = QString();
    if (containers[configType])
        configData = containers[configType]->toJson();

    QString message = QString();
    bool result, ok = false;
    if ( editMode )
        result = HttpReqListenerEdit(configName, configType, configData, authProfile, &message, &ok);
    else
        result = HttpReqListenerStart(configName, configType, configData, authProfile, &message, &ok);

    if( !result ) {
        MessageError("Response timeout");
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
    int typeIndex = listenerCombobox->findText( configType );
    if(typeIndex == -1 || !containers.contains(configType)) {
        MessageError("No such listener exists");
        return;
    }

    QString configData = jsonObject["config"].toString();
    listenerCombobox->setCurrentIndex(typeIndex);
    containers[configType]->fromJson(configData);
}

void DialogListener::onButtonSave()
{
    auto configName= inputListenerName->text();
    auto configType= listenerCombobox->currentText();
    auto configData = QString();
    if (containers[configType])
        configData = containers[configType]->toJson();

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

void DialogListener::onButtonCancel() { this->close(); }