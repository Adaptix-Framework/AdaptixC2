#include <UI/Dialogs/DialogAgent.h>
#include <Client/Requestor.h>

DialogAgent::DialogAgent(QString listenerName, QString listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

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
    listenerLabel->setText(QString::fromUtf8("Listener:"));

    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel = new QLabel(this);
    agentLabel->setText(QString::fromUtf8("Agent: "));

    agentCombobox = new QComboBox(this);

    configStackWidget = new QStackedWidget(this );

    stackGridLayout = new QGridLayout(this );
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    agentConfigGroupbox = new QGroupBox( this );
    agentConfigGroupbox->setTitle(QString::fromUtf8("Agent config") );
    agentConfigGroupbox->setLayout(stackGridLayout);

    generateButton = new QPushButton(this);
    generateButton->setText(QString::fromUtf8("Generate"));

    closeButton = new QPushButton(this);
    closeButton->setText(QString::fromUtf8("Close"));

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->addWidget( listenerLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget( listenerInput, 0, 1, 1, 5);
    mainGridLayout->addWidget( agentLabel, 1, 0, 1, 1);
    mainGridLayout->addWidget( agentCombobox, 1, 1, 1, 5);
    mainGridLayout->addItem(horizontalSpacer, 2, 0, 1, 6);
    mainGridLayout->addWidget( agentConfigGroupbox, 3, 0, 1, 6 );
    mainGridLayout->addItem( horizontalSpacer_2, 4, 0, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_4, 4, 1, 1, 1);
    mainGridLayout->addWidget( generateButton, 4, 2, 1, 1);
    mainGridLayout->addWidget( closeButton, 4, 3, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_5, 4, 4, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_3, 4, 5, 1, 1);

    this->setLayout(mainGridLayout);
}

void DialogAgent::Start()
{
    this->exec();
}

void DialogAgent::AddExAgents(QMap<QString, WidgetBuilder *> agents)
{
    agentsUI = agents;

    for (auto w : agentsUI.values()) {
        configStackWidget->addWidget( w->GetWidget() );
    }

    agentCombobox->clear();
    agentCombobox->addItems( agentsUI.keys() );
}

void DialogAgent::SetProfile(AuthProfile profile)
{
    this->authProfile = profile;
}

void DialogAgent::changeConfig(QString fn)
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

    this->close();
}

void DialogAgent::onButtonClose()
{
    this->close();
}
