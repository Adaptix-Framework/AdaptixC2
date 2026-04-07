#include <Agent/Agent.h>
#include <UI/MainUI.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogExtender.h>
#include <UI/Dialogs/DialogSettings.h>
#include <UI/Dialogs/DialogSubscriptions.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

#include <QDesktopServices>
#include <QUrl>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QApplication>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/DockWidget.h>

MainUI::MainUI()
{
    this->setWindowTitle( FRAMEWORK_VERSION );
    this->setProperty("Main", "base");

    auto newProjectAction = new QAction("New Project", this);
    connect(newProjectAction, &QAction::triggered, this, &MainUI::onNewProject);
    auto closeProjectAction = new QAction("Close Project", this);
    connect(closeProjectAction, &QAction::triggered, this, &MainUI::onCloseProject);

    auto projectSettingsAction = new QAction("Subscriptions", this);
    connect(projectSettingsAction, &QAction::triggered, this, &MainUI::onProjectSubscriptions);

    menuProject = new oclero::qlementine::Menu("Projects", this);
    menuProject->addAction(newProjectAction);
    menuProject->addAction(closeProjectAction);
    menuProject->addSeparator();
    menuProject->addAction(projectSettingsAction);

    auto axConsoleAction = new QAction("AxScript console ", this);
    connect(axConsoleAction, &QAction::triggered, this, &MainUI::onAxScriptConsole);
    auto scriptManagerAction = new QAction("Script manager", this);
    connect(scriptManagerAction, &QAction::triggered, this, &MainUI::onScriptManager);

    menuExtensions = new oclero::qlementine::Menu("Extensions", this);
    menuExtensions->addAction(axConsoleAction);
    menuExtensions->addAction(scriptManagerAction);
    extDocksSeparator = menuExtensions->addSeparator();
    extDocksSeparator->setVisible(false);

    menuSettings = new oclero::qlementine::Menu("Settings", this);
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
    mainuiTabWidget->tabBar()->setMovable(false);
    mainuiTabWidget->setMovable(false);

    connect(mainuiTabWidget, &QTabWidget::currentChanged, this, &MainUI::onTabChanged);

    this->setCentralWidget(mainuiTabWidget);

    qApp->installEventFilter(this);

    auto shortcutDockLeft = new QShortcut(QKeySequence("Ctrl+Left"), this);
    shortcutDockLeft->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockLeft, &QShortcut::activated, this, []() {
        auto* registry = KDDockWidgets::DockRegistry::self();
        if (!registry) return;
        auto* dw = registry->focusedDockWidget();
        if (!dw) return;
        for (auto* group : registry->groups()) {
            if (!group->containsDockWidget(dw)) continue;
            int count = group->dockWidgetCount();
            if (count <= 1) return;
            int idx = group->currentIndex();
            group->setCurrentTabIndex((idx - 1 + count) % count);
            return;
        }
    });

    auto shortcutDockRight = new QShortcut(QKeySequence("Ctrl+Right"), this);
    shortcutDockRight->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockRight, &QShortcut::activated, this, []() {
        auto* registry = KDDockWidgets::DockRegistry::self();
        if (!registry) return;
        auto* dw = registry->focusedDockWidget();
        if (!dw) return;
        for (auto* group : registry->groups()) {
            if (!group->containsDockWidget(dw)) continue;
            int count = group->dockWidgetCount();
            if (count <= 1) return;
            int idx = group->currentIndex();
            group->setCurrentTabIndex((idx + 1) % count);
            return;
        }
    });

    auto shortcutDockClose = new QShortcut(QKeySequence("Ctrl+D"), this);
    shortcutDockClose->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockClose, &QShortcut::activated, this, []() {
        auto* registry = KDDockWidgets::DockRegistry::self();
        if (!registry) return;
        auto* dw = registry->focusedDockWidget();
        if (!dw) return;
        dw->forceClose();
    });

    auto shortcutDockFloat = new QShortcut(QKeySequence("Ctrl+W"), this);
    shortcutDockFloat->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockFloat, &QShortcut::activated, this, []() {
        auto* registry = KDDockWidgets::DockRegistry::self();
        if (!registry) return;
        auto* dw = registry->focusedDockWidget();
        if (!dw) return;
        if (dw->isInMainWindow())
            dw->setFloating(true);
        else
            dw->setFloating(false);
    });

    auto isTerminalFocused = []() -> bool {
        auto* registry = KDDockWidgets::DockRegistry::self();
        if (!registry) return false;
        auto* dw = registry->focusedDockWidget();
        if (!dw) return false;
        QString name = dw->uniqueName();
        return name.startsWith("Terminal [") || name.startsWith("Shell [");
    };

    auto getCurrentAdaptixWidget = [this]() -> AdaptixWidget* {
        return qobject_cast<AdaptixWidget*>(mainuiTabWidget->currentWidget());
    };

    auto shortcutSessions = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    shortcutSessions->setContext(Qt::ApplicationShortcut);
    connect(shortcutSessions, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->SetSessionsTableUI();
    });

    auto shortcutGraph = new QShortcut(QKeySequence("Ctrl+Shift+G"), this);
    shortcutGraph->setContext(Qt::ApplicationShortcut);
    connect(shortcutGraph, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->SetGraphUI();
    });

    auto shortcutListeners = new QShortcut(QKeySequence("Ctrl+Shift+L"), this);
    shortcutListeners->setContext(Qt::ApplicationShortcut);
    connect(shortcutListeners, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadListenersUI();
    });

    auto shortcutLogs = new QShortcut(QKeySequence("Ctrl+Shift+N"), this);
    shortcutLogs->setContext(Qt::ApplicationShortcut);
    connect(shortcutLogs, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadLogsUI();
    });

    auto shortcutTasks = new QShortcut(QKeySequence("Ctrl+Shift+J"), this);
    shortcutTasks->setContext(Qt::ApplicationShortcut);
    connect(shortcutTasks, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->SetTasksUI();
    });

    auto shortcutScriptManager = new QShortcut(QKeySequence("Ctrl+Shift+E"), this);
    shortcutScriptManager->setContext(Qt::ApplicationShortcut);
    connect(shortcutScriptManager, &QShortcut::activated, this, [this, isTerminalFocused]() {
        if (isTerminalFocused()) return;
        this->onScriptManager();
    });

    auto shortcutDownloads = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    shortcutDownloads->setContext(Qt::ApplicationShortcut);
    connect(shortcutDownloads, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadDownloadsUI();
    });

    auto shortcutTargets = new QShortcut(QKeySequence("Ctrl+Shift+T"), this);
    shortcutTargets->setContext(Qt::ApplicationShortcut);
    connect(shortcutTargets, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadTargetsUI();
    });

    auto shortcutCreds = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    shortcutCreds->setContext(Qt::ApplicationShortcut);
    connect(shortcutCreds, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadCredentialsUI();
    });

    auto shortcutTunnels = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    shortcutTunnels->setContext(Qt::ApplicationShortcut);
    connect(shortcutTunnels, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadTunnelsUI();
    });

    auto shortcutScreenshots = new QShortcut(QKeySequence("Ctrl+Shift+I"), this);
    shortcutScreenshots->setContext(Qt::ApplicationShortcut);
    connect(shortcutScreenshots, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w)
            w->LoadScreenshotsUI();
    });

    auto shortcutSettings = new QShortcut(QKeySequence("Ctrl+Shift+R"), this);
    shortcutSettings->setContext(Qt::ApplicationShortcut);
    connect(shortcutSettings, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        MainUI::onSettings();
    });
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

bool MainUI::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->modifiers() == Qt::ControlModifier &&
            (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right)) {
            event->ignore();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
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

    QString tabName = profile->GetProject();
    int id = mainuiTabWidget->addTab(adaptixWidget, tabName);
    mainuiTabWidget->setTabToolTip(id, tabName);
    mainuiTabWidget->setCurrentIndex( id );

    AdaptixProjects.append(adaptixWidget);

    updateTabButton(id, tabName, true);
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

QVector<AdaptixWidget*> MainUI::GetAdaptixProjects() const
{
    return AdaptixProjects;
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

void MainUI::onScriptManager()
{
    GlobalClient->extender->dialogExtender->SetMainUI(this);
    GlobalClient->extender->dialogExtender->show();
}

void MainUI::onSettings() { GlobalClient->settings->getDialogSettings()->show(); }

void MainUI::onProjectSubscriptions()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>(mainuiTabWidget->currentWidget());
    if (!adaptixWidget)
        return;

    auto dialog = new DialogSubscriptions(adaptixWidget, this);
    dialog->SetRegisteredCategories(adaptixWidget->GetProfile()->GetRegisteredCategories());
    dialog->SetCurrentSubscriptions(adaptixWidget->GetProfile()->GetSubscriptions());
    dialog->SetConsoleMultiuser(adaptixWidget->GetProfile()->GetConsoleMultiuser());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

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

void MainUI::onOpenProjectDirectory()
{
    AuthProfile* profile = GetCurrentProfile();
    if (!profile)
        return;

    QString projectDir = profile->GetProjectDir();
    if (projectDir.isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromLocalFile(projectDir));
}

void MainUI::onTabChanged(int index)
{
    for (int i = 0; i < mainuiTabWidget->count(); ++i) {
        auto adaptixWidget = qobject_cast<AdaptixWidget*>(mainuiTabWidget->widget(i));
        if (adaptixWidget) {
            QString tabName = adaptixWidget->GetProfile()->GetProject();
            bool showButton = (i == index);
            updateTabButton(i, tabName, showButton);
        }
    }
}

void MainUI::updateTabButton(const int index, const QString& tabName, const bool showButton)
{
    if (index < 0 || index >= mainuiTabWidget->count())
        return;

    QTabBar* tabBar = mainuiTabWidget->tabBar();

    QString realName = mainuiTabWidget->tabText(index);
    if (realName.isEmpty())
        realName = tabName;

    tabBar->setTabText(index, "");

    QWidget* existingWidget = tabBar->tabButton(index, QTabBar::RightSide);
    if (existingWidget) {
        existingWidget->deleteLater();
        tabBar->setTabButton(index, QTabBar::RightSide, nullptr);
    }

    auto* tabWidget = new QWidget();
    auto* layout = new QHBoxLayout(tabWidget);
    layout->setContentsMargins(showButton ? 12 : 16, 0, showButton ? 6 : 12, 0);
    layout->setSpacing(1);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    tabWidget->setFixedHeight(24);
    tabWidget->setMinimumHeight(24);
    tabWidget->setMaximumHeight(24);
    tabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* label = new QLabel(realName);
    label->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label->setFixedHeight(24);
    label->setMinimumHeight(24);
    label->setMaximumHeight(24);
    QFont labelFont = label->font();
    QFont appFont = QApplication::font();
    labelFont.setFamily(appFont.family());
    labelFont.setPointSize(appFont.pointSize());
    label->setFont(labelFont);
    label->setProperty("transparent", true);

    layout->addWidget(label, 1, Qt::AlignCenter);

    if (showButton) {
        auto* folderButton = new QPushButton(tabWidget);
        folderButton->setIcon(QIcon(":/icons/folder"));

        constexpr int iconSize = 14;
        constexpr int buttonHeight = 16;
        constexpr int buttonWidth = 24;

        folderButton->setIconSize(QSize(iconSize, iconSize));
        folderButton->setFixedSize(buttonWidth, buttonHeight);
        folderButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        folderButton->setToolTip("Open project directory");
        folderButton->setFlat(true);

        connect(folderButton, &QPushButton::clicked, this, &MainUI::onOpenProjectDirectory);

        layout->addWidget(folderButton, 0, Qt::AlignRight);
    }

    tabBar->setTabButton(index, QTabBar::RightSide, tabWidget);
}