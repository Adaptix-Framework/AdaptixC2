#include <UI/MainUI.h>
#include <MainAdaptix.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );

    auto newProjectAction = new QAction("New Project", this);
//    connect(newProjectAction, &QAction::triggered, this, &MainUI::onNewProject);
    auto closeProjectAction = new QAction("Close Project", this);
//    connect(newProjectAction, &QAction::triggered, this, &MainUI::onCloseProject);

    auto menuProject = new QMenu("Projects", this);
    menuProject->addAction(newProjectAction);
    menuProject->addAction(closeProjectAction);

    auto extenderAction = new QAction("Extender", this);
    connect(extenderAction, &QAction::triggered, this, &MainUI::onExtender);

    auto settingsAction = new QAction("Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainUI::onSettings);

    menuProject->setEnabled(false);

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

void MainUI::onExtender()
{
    GlobalClient->extender->dialogExtender->show();
}

void MainUI::onSettings()
{
    GlobalClient->settings->dialogSettings->show();
}

void MainUI::AddNewProject(AuthProfile* profile)
{
    mainWidget = new AdaptixWidget(profile);

    for (auto extFile : GlobalClient->extender->extenderFiles){
        if(extFile.Valid && extFile.Enabled)
            this->AddNewExtension(extFile);
    }

    this->setCentralWidget(mainWidget);
}

void MainUI::AddNewExtension(ExtensionFile extFile)
{
    if (mainWidget) {
        auto adaptixWidget = qobject_cast<AdaptixWidget *>(mainWidget);
        adaptixWidget->AddExtension(extFile);
    }
}

void MainUI::RemoveExtension(ExtensionFile extFile)
{
    if (mainWidget) {
        auto adaptixWidget = qobject_cast<AdaptixWidget *>(mainWidget);
        adaptixWidget->RemoveExtension(extFile);
    }
}
