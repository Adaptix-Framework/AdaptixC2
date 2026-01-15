#include <Agent/Agent.h>
#include <UI/MainUI.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogExtender.h>
#include <UI/Dialogs/DialogSettings.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );
    this->setProperty("Main", "base");

    auto newProjectAction = new QAction("New Project", this);
    connect(newProjectAction, &QAction::triggered, this, &MainUI::onNewProject);
    auto closeProjectAction = new QAction("Close Project", this);
    connect(closeProjectAction, &QAction::triggered, this, &MainUI::onCloseProject);

    menuProject = new QMenu("Projects", this);
    menuProject->addAction(newProjectAction);
    menuProject->addAction(closeProjectAction);

    auto axConsoleAction = new QAction("AxScript console ", this);
    connect(axConsoleAction, &QAction::triggered, this, &MainUI::onAxScriptConsole);
    auto scriptManagerAction = new QAction("Script manager", this);
    connect(scriptManagerAction, &QAction::triggered, this, &MainUI::onScriptManager);

    menuExtensions = new QMenu("Extensions", this);
    menuExtensions->addAction(axConsoleAction);
    menuExtensions->addAction(scriptManagerAction);
    extDocksSeparator = menuExtensions->addSeparator();
    extDocksSeparator->setVisible(false);

    menuSettings = new QMenu("Settings", this);
    auto settingsAction = new QAction("Open settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainUI::onSettings);
    menuSettings->addAction(settingsAction);

    auto mainMenuBar = new QMenuBar(this);
    mainMenuBar->addMenu(menuProject);
    mainMenuBar->addMenu(menuExtensions);
    mainMenuBar->addMenu(menuSettings);

    this->setMenuBar(mainMenuBar);

    mainuiTabWidget = new QTabWidget();
    mainuiTabWidget->setTabPosition(QTabWidget::South);
    mainuiTabWidget->tabBar()->setMovable(true);
    mainuiTabWidget->setMovable(true);

    this->setCentralWidget(mainuiTabWidget);
}

MainUI::~MainUI()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget) {
            disconnect(adaptixWidget, nullptr, nullptr, nullptr);
            adaptixWidget->Close();
        }
    }
    qDeleteAll(AdaptixProjects);
    AdaptixProjects.clear();
}

void MainUI::closeEvent(QCloseEvent* event)
{
    QCoreApplication::quit();
    event->accept();
}

void MainUI::AddNewProject(AuthProfile* profile, QThread* channelThread, WebSocketWorker* channelWsWorker)
{
    auto adaptixWidget = new AdaptixWidget(profile, channelThread, channelWsWorker);
    connect(adaptixWidget, &AdaptixWidget::SyncedOnReloadSignal,   GlobalClient->extender, &Extender::syncedOnReload);
    connect(adaptixWidget, &AdaptixWidget::LoadGlobalScriptSignal, GlobalClient->extender, &Extender::loadGlobalScript);
    connect(adaptixWidget, &AdaptixWidget::UnloadGlobalScriptSignal, GlobalClient->extender, &Extender::unloadGlobalScript);

    QString tabName = "   " + profile->GetProject() + "   " ;
    int id = mainuiTabWidget->addTab( adaptixWidget, tabName);
    mainuiTabWidget->setCurrentIndex( id );

    AdaptixProjects.append(adaptixWidget);
}

bool MainUI::AddNewExtension(ExtensionFile *extFile)
{
    bool result = true;
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget) {
            result = adaptixWidget->AddExtension(extFile);
            if (!result)
                break;
        }
    }
    return result;
}

bool MainUI::SyncExtension(const QString &Project, ExtensionFile *extFile)
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget && adaptixWidget->GetProfile()->GetProject() == Project)
            return adaptixWidget->AddExtension(extFile);
    }
    return true;
}

void MainUI::RemoveExtension(const ExtensionFile &extFile)
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->RemoveExtension(extFile);
    }
}

void MainUI::UpdateSessionsTableColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->SessionsTableDock->UpdateColumnsVisible();
    }
}

void MainUI::UpdateGraphIcons() {
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget) {
            for (auto agent : adaptixWidget->AgentsMap.values() ) {
                agent->UpdateImage();
            }
            adaptixWidget->SessionsGraphDock->UpdateIcons();
        }
    }
}

void MainUI::UpdateTasksTableColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->TasksDock->UpdateColumnsVisible();
    }
}

AuthProfile* MainUI::GetCurrentProfile() const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainuiTabWidget->currentWidget() );
    if (!adaptixWidget)
        return nullptr;
    return adaptixWidget->GetProfile();
}

/// Actions

void MainUI::onNewProject() { GlobalClient->NewProject(); }

void MainUI::onCloseProject()
{
    int currentIndex = mainuiTabWidget->currentIndex();
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainuiTabWidget->currentWidget() );
    if (!adaptixWidget)
        return;

    for (int i = 0; i < AdaptixProjects.size(); ++i) {
        if (AdaptixProjects[i] == adaptixWidget) {
            AdaptixProjects.remove(i);
            break;
        }
    }

    adaptixWidget->Close();
    delete adaptixWidget;

    mainuiTabWidget->removeTab(currentIndex);
}

void MainUI::onAxScriptConsole()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainuiTabWidget->currentWidget() );
    if (!adaptixWidget)
        return;

    adaptixWidget->LoadAxConsoleUI();
}

void MainUI::onScriptManager() { GlobalClient->extender->dialogExtender->show(); }

void MainUI::onSettings() { GlobalClient->settings->getDialogSettings()->show(); }

/// EXT MENU

QMenu* MainUI::getMenuProject() const { return menuProject; }

QMenu* MainUI::getMenuAxScript() const { return menuExtensions; }

QMenu* MainUI::getMenuSettings() const { return menuSettings; }

void MainUI::addExtDockAction(const QString &id, const QString &title, bool checked, const std::function<void(bool)> &callback)
{
    if (extDockActions.contains(id))
        return;

    auto *action = new QAction(title, this);
    action->setCheckable(true);
    action->setChecked(checked);
    connect(action, &QAction::toggled, this, callback);
    menuExtensions->addAction(action);
    extDockActions[id] = action;

    if (extDocksSeparator && !extDocksSeparator->isVisible())
        extDocksSeparator->setVisible(true);
}

void MainUI::removeExtDockAction(const QString &id)
{
    if (!extDockActions.contains(id))
        return;

    auto *action = extDockActions.take(id);
    menuExtensions->removeAction(action);
    delete action;

    if (extDockActions.isEmpty() && extDocksSeparator)
        extDocksSeparator->setVisible(false);
}

void MainUI::setExtDockChecked(const QString &id, bool checked)
{
    if (extDockActions.contains(id))
        extDockActions[id]->setChecked(checked);
}
