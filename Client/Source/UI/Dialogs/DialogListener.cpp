#include <UI/Dialogs/DialogListener.h>
#include <Client/Requestor.h>

DialogListener::DialogListener()
{
    this->createUI();

    connect(listenerTypeCombobox, &QComboBox::currentTextChanged, this, &DialogListener::changeConfig);
    connect( buttonSave, &QPushButton::clicked, this, &DialogListener::onButtonSave );
    connect( buttonClose, &QPushButton::clicked, this, &DialogListener::onButtonClose );
}

DialogListener::~DialogListener() = default;

void DialogListener::createUI()
{
    this->resize( 650, 650 );
    this->setWindowTitle( "Create Listener" );

    listenerNameLabel = new QLabel(this);
    listenerNameLabel->setText(QString::fromUtf8("Listener name:"));

    listenerNameInput = new QLineEdit(this);

    listenerTypeLabel = new QLabel(this);
    listenerTypeLabel->setText(QString::fromUtf8("Listener type: "));

    listenerTypeCombobox = new QComboBox(this);

    configStackWidget = new QStackedWidget(this );

    stackGridLayout = new QGridLayout(this );
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    listenerConfigGroupbox = new QGroupBox( this );
    listenerConfigGroupbox->setTitle(QString::fromUtf8("Listener config") );
    listenerConfigGroupbox->setLayout(stackGridLayout);

    buttonSave = new QPushButton(this);
    buttonSave->setText(QString::fromUtf8("Save"));

    buttonClose = new QPushButton(this);
    buttonClose->setText(QString::fromUtf8("Close"));

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->addWidget( listenerNameLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget( listenerNameInput, 0, 1, 1, 5);
    mainGridLayout->addWidget( listenerTypeLabel, 1, 0, 1, 1);
    mainGridLayout->addWidget( listenerTypeCombobox, 1, 1, 1, 5);
    mainGridLayout->addItem(horizontalSpacer, 2, 0, 1, 6);
    mainGridLayout->addWidget( listenerConfigGroupbox, 3, 0, 1, 6 );
    mainGridLayout->addItem( horizontalSpacer_2, 4, 0, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_4, 4, 1, 1, 1);
    mainGridLayout->addWidget( buttonSave, 4, 2, 1, 1);
    mainGridLayout->addWidget( buttonClose, 4, 3, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_5, 4, 4, 1, 1);
    mainGridLayout->addItem( horizontalSpacer_3, 4, 5, 1, 1);

    this->setLayout(mainGridLayout);
}

void DialogListener::Start()
{
    this->exec();
}

void DialogListener::AddExListeners(QMap<QString, WidgetBuilder*> listeners)
{
    listenersUI = listeners;

    for (auto w : listenersUI.values()) {
        configStackWidget->addWidget( w->GetWidget() );
    }

    listenerTypeCombobox->clear();
    listenerTypeCombobox->addItems( listenersUI.keys() );
}

void DialogListener::SetProfile(AuthProfile profile)
{
    this->authProfile = profile;
}

void DialogListener::SetEditMode(QString name)
{
    this->setWindowTitle( "Edit Listener" );
    listenerNameInput->setText(name);
    listenerNameInput->setDisabled(true);
    listenerTypeCombobox->setDisabled(true);
    editMode = true;
}

void DialogListener::changeConfig(QString fn)
{
    if (listenersUI[fn]) {
        auto w = listenersUI[fn]->GetWidget();
        configStackWidget->setCurrentWidget(w);
    }
}

void DialogListener::onButtonSave()
{
    auto configName= listenerNameInput->text();
    auto configType= listenerTypeCombobox->currentText();
    auto configData = QString();
    if (listenersUI[configType]) {
        configData = listenersUI[configType]->CollectData();
    }

    QString message = QString();
    bool result, ok = false;
    if ( editMode ) {
        result = HttpReqListenerEdit(configName, configType, configData, authProfile, &message, &ok);
    }
    else {
        result = HttpReqListenerStart(configName, configType, configData, authProfile, &message, &ok);
    }

    if( !result ){
        MessageError("JWT error");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }

    this->close();
}

void DialogListener::onButtonClose()
{
    this->close();
}