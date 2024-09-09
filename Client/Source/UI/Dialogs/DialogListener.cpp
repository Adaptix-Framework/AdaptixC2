#include <UI/Dialogs/DialogListener.h>

DialogListener::DialogListener() {
    this->createUI();
}

DialogListener::~DialogListener() {

}

void DialogListener::createUI() {
    this->resize( 650, 650 );
    this->setWindowTitle( "Create Listener" );
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "DialogListener" ) );

    listenerNameLabel = new QLabel(this);
    listenerNameLabel->setText(QString::fromUtf8("Listener name:"));

    listenerNameInput = new QLineEdit(this);

    listenerTypeLabel = new QLabel(this);
    listenerTypeLabel->setText(QString::fromUtf8("Listener type: "));

    listenerTypeCombobox = new QComboBox(this);
    listenerTypeCombobox->setObjectName(QString::fromUtf8("TypeComboboxDialogListener" ) );

    configStackWidget = new QStackedWidget(this );
    configStackWidget->setObjectName(QString::fromUtf8("ConfigStackDialogListener" ));

    stackGridLayout = new QGridLayout(this );
    stackGridLayout->setObjectName(QString::fromUtf8("ConfigLayoutDialogListener" ));
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    listenerConfigGroupbox = new QGroupBox( this );
    listenerConfigGroupbox->setObjectName(QString::fromUtf8("ConfigGroupboxDialogListener"));
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
    mainGridLayout->setObjectName(QString::fromUtf8("MainLayoutDialogListener"));
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

void DialogListener::Start( ) {
    this->exec();
}