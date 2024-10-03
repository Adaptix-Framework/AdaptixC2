#include <UI/Dialogs/DialogAgent.h>

DialogAgent::DialogAgent()
{
    this->createUI();
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->resize( 450, 450 );
    this->setWindowTitle( "Generate Agent" );
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "DialogAgent" ) );

    listenerLabel = new QLabel(this);
    listenerLabel->setText(QString::fromUtf8("Listener:"));

    listenerInput = new QLineEdit(this);

    agentLabel = new QLabel(this);
    agentLabel->setText(QString::fromUtf8("Agent: "));

    agentCombobox = new QComboBox(this);
    agentCombobox->setObjectName(QString::fromUtf8("AgentComboboxDialogAgent" ) );

    configStackWidget = new QStackedWidget(this );
    configStackWidget->setObjectName(QString::fromUtf8("ConfigStackDialogListener" ));

    stackGridLayout = new QGridLayout(this );
    stackGridLayout->setObjectName(QString::fromUtf8("ConfigLayoutDialogAgent" ));
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    agentConfigGroupbox = new QGroupBox( this );
    agentConfigGroupbox->setObjectName(QString::fromUtf8("ConfigGroupboxDialogListener"));
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
    mainGridLayout->setObjectName(QString::fromUtf8("MainLayoutDialogAgent"));
    mainGridLayout->addWidget( listenerLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget( listenerInput, 0, 1, 1, 5);
    mainGridLayout->addWidget( agentLabel, 1, 0, 1, 1);
    mainGridLayout->addWidget( agentCombobox, 1, 1, 1, 5);
    mainGridLayout->addItem(horizontalSpacer, 2, 0, 1, 6);
    mainGridLayout->addWidget( agentConfigGroupbox, 3, 0, 1, 6 );
    mainGridLayout->addItem( horizontalSpacer_2, 4, 0, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_4, 4, 1, 1, 1);
    mainGridLayout->addWidget(generateButton, 4, 2, 1, 1);
    mainGridLayout->addWidget(closeButton, 4, 3, 1, 1);
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
    if (agentsUI[fn]) {
        auto w = agentsUI[fn]->GetWidget();
        configStackWidget->setCurrentWidget(w);
    }
}

void DialogAgent::onButtonGenerate()
{

}

void DialogAgent::onButtonClose()
{
    this->close();
}
