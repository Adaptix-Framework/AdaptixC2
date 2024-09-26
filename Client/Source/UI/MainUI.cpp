#include <UI/MainUI.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "MainUI" ) );
}

void MainUI::addNewProject(AuthProfile profile)
{
    mainWidget = new AdaptixWidget(profile);
    this->setCentralWidget(mainWidget);
}

MainUI::~MainUI()
{
    delete mainWidget;
}
