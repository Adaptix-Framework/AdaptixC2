#include <UI/Dialogs/DialogAgent.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxElementWrappers.h>

DialogAgent::DialogAgent(const QString &listenerName, const QString &listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect(buttonLoad,     &QPushButton::clicked,          this, &DialogAgent::onButtonLoad);
    connect(buttonSave,     &QPushButton::clicked,          this, &DialogAgent::onButtonSave);
    connect(agentCombobox,  &QComboBox::currentTextChanged, this, &DialogAgent::changeConfig);
    connect(generateButton, &QPushButton::clicked,          this, &DialogAgent::onButtonGenerate);
    connect(closeButton,    &QPushButton::clicked,          this, &DialogAgent::onButtonClose);
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->setWindowTitle( "Generate Agent" );
    this->setProperty("Main", "base");

    listenerLabel = new QLabel("Listener:", this);
    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel    = new QLabel("Agent: ", this);
    agentCombobox = new QComboBox(this);

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setIconSize( QSize( 25,25 ));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
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
    generateButton->setProperty( "ButtonStyle", "dialog" );

    closeButton = new QPushButton( "Close", this);
    closeButton->setProperty( "ButtonStyle", "dialog" );

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
    this->setModal(true);
    this->show();
}

void DialogAgent::AddExAgents(const QStringList &agents, const QMap<QString, AxUI> &uis)
{
    agentCombobox->clear();

    this->agents = agents;
    this->ax_uis = uis;

    for (auto agent : agents) {
        auto ax_ui = &this->ax_uis[agent];
        ax_ui->widget->setParent(nullptr);
        ax_ui->widget->setParent(this);
        ax_ui->container->setParent(nullptr);
        ax_ui->container->setParent(this);

        configStackWidget->addWidget(ax_ui->widget);

        agentCombobox->addItem(agent);
    }
 }

void DialogAgent::SetProfile(const AuthProfile &profile) { this->authProfile = profile; }

void DialogAgent::onButtonGenerate()
{
    QString agentName  = agentCombobox->currentText();
    auto configData = QString();

    if (ax_uis.contains(agentName) && ax_uis[agentName].container)
        configData = ax_uis[agentName].container->toJson();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqAgentGenerate(listenerName, listenerType, agentName, configData, authProfile, &message, &ok);
    if( !result ){
        MessageError("Server is not responding");
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

    QString baseDir = authProfile.GetProjectDir();
    QString initialPath = QDir(baseDir).filePath(filename);

    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "All Files (*.*)",
        [this, content](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(content);
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
    });
}

void DialogAgent::onButtonLoad()
{
    QString baseDir = authProfile.GetProjectDir();
    NonBlockingDialogs::getOpenFileName(this, "Select file", baseDir, "JSON files (*.json)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
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
            if ( typeIndex == -1 ) {
                MessageError("No such agent exists");
                return;
            }
            agentCombobox->setCurrentIndex(typeIndex);
            this->changeConfig(agentType);

            QString configData = jsonObject["config"].toString();

            ax_uis[agentType].container->fromJson(configData);
    });
}

void DialogAgent::onButtonSave()
{
    QString configType = agentCombobox->currentText();
    auto configData = QString();
    if (ax_uis.contains(configType) && ax_uis[configType].container)
        configData = ax_uis[configType].container->toJson();

    QJsonObject dataJson;
    dataJson["listener_type"] = listenerType;
    dataJson["agent"]         = configType;
    dataJson["config"]        = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = QString("%1_config.json").arg(configType);
    QString baseDir = authProfile.GetProjectDir();
    QString initialPath = QDir(baseDir).filePath(tmpFilename);
    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "JSON files (*.json)",
        [this, fileContent](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(fileContent);
            file.close();

            QInputDialog inputDialog;
            inputDialog.setWindowTitle("Save config");
            inputDialog.setLabelText("File saved to:");
            inputDialog.setTextEchoMode(QLineEdit::Normal);
            inputDialog.setTextValue(filePath);
            inputDialog.adjustSize();
            inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
            inputDialog.exec();
    });
}

void DialogAgent::onButtonClose() { this->close(); }

void DialogAgent::changeConfig(const QString &agentName)
{
    if (ax_uis.contains(agentName)) {
        auto ax_ui = &ax_uis[agentName];
        if (ax_ui)
            configStackWidget->setCurrentWidget(ax_ui->widget);
        this->resize(ax_ui->width, ax_ui->height);
    }
}