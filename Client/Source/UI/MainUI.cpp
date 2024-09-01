#include <UI/MainUI.h>

MainUI::MainUI() {
    this->setWindowTitle( FRAMEWORK_VERSION );
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "MainUI" ) );

    mainWidget = new AdaptixWidget;
    this->setCentralWidget(mainWidget);
}
