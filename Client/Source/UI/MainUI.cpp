#include <UI/MainUI.h>
#include <MainAdaptix.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "MainUI" ) );

    auto newProjectAction = new QAction("New Project", this);
    auto closeProjectAction = new QAction("Close Project", this);

    auto menuProject = new QMenu("Project", this);
    menuProject->addAction(newProjectAction);
    menuProject->addAction(closeProjectAction);

    auto extenderAction = new QAction("Extender", this);
    connect(extenderAction, &QAction::triggered, this, &MainUI::onExtender);

    auto settingsAction = new QAction("Settings", this);

    menuProject->setEnabled(false);
    settingsAction->setEnabled(false);

    auto mainMenuBar = new QMenuBar(this);
    mainMenuBar->addMenu(menuProject);
    mainMenuBar->addAction(extenderAction);
    mainMenuBar->addAction(settingsAction);
    this->setMenuBar(mainMenuBar);
}

MainUI::~MainUI()
{
    delete mainWidget;
}

void MainUI::addNewProject(AuthProfile* profile)
{
    mainWidget = new AdaptixWidget(profile);
    this->setCentralWidget(mainWidget);
}

void MainUI::onExtender()
{
    GlobalClient->extender->dialogExtender->show();
}
