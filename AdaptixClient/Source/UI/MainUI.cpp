#include <UI/MainUI.h>
#include <MainAdaptix.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );

    auto newProjectAction = new QAction("New Project", this);
    connect(newProjectAction, &QAction::triggered, this, &MainUI::onNewProject);
    auto closeProjectAction = new QAction("Close Project", this);
    connect(closeProjectAction, &QAction::triggered, this, &MainUI::onCloseProject);

    auto menuProject = new QMenu("Projects", this);
    menuProject->addAction(newProjectAction);
    menuProject->addAction(closeProjectAction);

    auto extenderAction = new QAction("Extender", this);
    connect(extenderAction, &QAction::triggered, this, &MainUI::onExtender);

    auto settingsAction = new QAction("Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainUI::onSettings);

    auto mainMenuBar = new QMenuBar(this);
    mainMenuBar->addMenu(menuProject);
    mainMenuBar->addAction(extenderAction);
    mainMenuBar->addAction(settingsAction);
    this->setMenuBar(mainMenuBar);

    mainuiTabWidget = new QTabWidget();
    mainuiTabWidget->setTabPosition(QTabWidget::South);
    mainuiTabWidget->tabBar()->setMovable(true);
    mainuiTabWidget->setMovable(true);

    this->setCentralWidget(mainuiTabWidget);
}

MainUI::~MainUI()
{
    for (auto project : AdaptixProjects.keys()) {
        delete AdaptixProjects[project];
        AdaptixProjects.remove(project);
    }
}

void MainUI::AddNewProject(AuthProfile* profile)
{
    auto adaptixWidget = new AdaptixWidget(profile);

    for (auto extFile : GlobalClient->extender->extenderFiles){
        if(extFile.Valid && extFile.Enabled)
            adaptixWidget->AddExtension(extFile);
    }

    QString tabName = "   " + profile->GetProject() + "   " ;
    int id = mainuiTabWidget->addTab( adaptixWidget, tabName);
    mainuiTabWidget->setCurrentIndex( id );

    AdaptixProjects[profile->GetProject()] = adaptixWidget;
}

void MainUI::AddNewExtension(ExtensionFile extFile)
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->AddExtension(extFile);
    }
}

void MainUI::RemoveExtension(ExtensionFile extFile)
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->RemoveExtension(extFile);
    }
}

/// Actions

void MainUI::onNewProject()
{
    GlobalClient->NewProject();
}

void MainUI::onCloseProject()
{
    int currentIndex = mainuiTabWidget->currentIndex();
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainuiTabWidget->currentWidget() );
    if (adaptixWidget) {
        AdaptixProjects.remove(adaptixWidget->GetProfile()->GetProject());
        adaptixWidget->Close();
        delete adaptixWidget;
    }
    mainuiTabWidget->removeTab(currentIndex);
}

void MainUI::onExtender()
{
    GlobalClient->extender->dialogExtender->show();
}

void MainUI::onSettings()
{
    GlobalClient->settings->dialogSettings->show();
}
