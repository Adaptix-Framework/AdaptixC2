#include <Agent/Agent.h>
#include <UI/MainUI.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/SessionWidgetIface.h>
#include <UI/Widgets/SessionsFeedWidget.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/TargetWidgetIface.h>
#include <UI/Widgets/CredentialWidgetIface.h>
#include <UI/Widgets/TargetsFeedWidget.h>
#include <UI/Widgets/CredentialsFeedWidget.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <UI/Widgets/PayloadsFeedWidget.h>
#include <Utils/CustomElements/ListFeed.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogSettings.h>
#include <UI/Dialogs/DialogSubscriptions.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>
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

    mainuiTabWidget = new QTabWidget();
    mainuiTabWidget->setTabPosition(QTabWidget::South);
    mainuiTabWidget->tabBar()->setMovable(true);
    mainuiTabWidget->setMovable(true);
    mainuiTabWidget->tabBar()->setMovable(false);
    mainuiTabWidget->setMovable(false);

    connect(mainuiTabWidget, &QTabWidget::currentChanged, this, &MainUI::onTabChanged);

    this->setCentralWidget(mainuiTabWidget);

    newProjectButton = new QPushButton(QIcon(":/icons/plus"), "", this);
    newProjectButton->setIconSize(QSize(20, 20));
    newProjectButton->setFixedSize(37, 28);
    newProjectButton->setToolTip("New Project");
    mainuiTabWidget->setCornerWidget(newProjectButton, Qt::TopLeftCorner);
    connect(newProjectButton, &QPushButton::clicked, this, [](){ GlobalClient->NewProject(); });

    mainuiTabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mainuiTabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &MainUI::onTabContextMenu);

    qApp->installEventFilter(this);

    auto shortcutDockPrev = new QShortcut(QKeySequence("Ctrl+PgUp"), this);
    shortcutDockPrev->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockPrev, &QShortcut::activated, this, []() {
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

    auto shortcutDockNext = new QShortcut(QKeySequence("Ctrl+PgDown"), this);
    shortcutDockNext->setContext(Qt::ApplicationShortcut);
    connect(shortcutDockNext, &QShortcut::activated, this, []() {
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

    auto shortcutPayloads = new QShortcut(QKeySequence("Ctrl+Shift+A"), this);
    shortcutPayloads->setContext(Qt::ApplicationShortcut);
    connect(shortcutPayloads, &QShortcut::activated, this, [=]() {
        if (isTerminalFocused()) return;
        auto* w = getCurrentAdaptixWidget();
        if (w) w->LoadPayloadsUI();
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
        this->onScriptsDock();
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
        if (ke->modifiers() == Qt::ControlModifier) {
            const int k = ke->key();
            const bool isDockNavKey = (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_D || k == Qt::Key_W);
            if (isDockNavKey) {
                auto* registry = KDDockWidgets::DockRegistry::self();
                bool terminalFocused = false;
                if (registry) {
                    if (auto* dw = registry->focusedDockWidget()) {
                        QString name = dw->uniqueName();
                        terminalFocused = name.startsWith("Terminal [") || name.startsWith("Shell [");
                    }
                }
                if (terminalFocused) {
                    event->accept();
                    return true;
                }

                event->ignore();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainUI::closeEvent(QCloseEvent* event)
{
    for (auto* adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->Close();
    }
    event->accept();
    QCoreApplication::quit();
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

void MainUI::UpdateConsolePrefs()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (!adaptixWidget)
            continue;
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (auto agent : adaptixWidget->AgentsMap.values()) {
            if (agent && agent->Console)
                agent->Console->ApplyConsolePrefs();
        }
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

void MainUI::UpdateTargetsColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget && adaptixWidget->TargetsDock)
            adaptixWidget->TargetsDock->UpdateColumnsVisible();
    }
}

void MainUI::UpdateCredentialsColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget && adaptixWidget->CredentialsDock)
            adaptixWidget->CredentialsDock->UpdateColumnsVisible();
    }
}

void MainUI::UpdateFilesColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget && adaptixWidget->DownloadsDock)
            adaptixWidget->DownloadsDock->UpdateColumnsVisible();
    }
}

void MainUI::UpdatePayloadsColumns()
{
    for (auto adaptixWidget : AdaptixProjects) {
        if (adaptixWidget && adaptixWidget->PayloadsDock)
            adaptixWidget->PayloadsDock->UpdateColumnsVisible();
    }
}

void MainUI::ApplyFeedViewPreferences()
{
    if (!GlobalClient || !GlobalClient->settings)
        return;
    const auto& d = GlobalClient->settings->data;

    for (auto* adaptixWidget : AdaptixProjects) {
        if (!adaptixWidget)
            continue;

        if (auto* sessions = qobject_cast<SessionsFeedWidget*>(adaptixWidget->SessionsTableDock ? adaptixWidget->SessionsTableDock->asWidget() : nullptr)) {
            sessions->setCompactMode(d.SessionsCompactMode);
            if (sessions->activeFilter())
                sessions->activeFilter()->setChecked(d.SessionsAutoHideInactive);
            sessions->onFilterChanged();
        }

        if (auto* tasks = adaptixWidget->TasksDock) {
            tasks->setCompactMode(d.TasksCompactMode);
            if (tasks->activeFilter())
                tasks->activeFilter()->setChecked(d.TasksInProcessOnly);
            tasks->onFilterChanged();
        }

        if (auto* targets = qobject_cast<TargetsFeedWidget*>(adaptixWidget->TargetsDock ? adaptixWidget->TargetsDock->asWidget() : nullptr)) {
            targets->setCompactMode(d.TargetsCompactMode);
        }

        if (auto* creds = qobject_cast<CredentialsFeedWidget*>(adaptixWidget->CredentialsDock ? adaptixWidget->CredentialsDock->asWidget() : nullptr)) {
            creds->setCompactMode(d.CredentialsCompactMode);
        }

        if (adaptixWidget->DownloadsDock)
            adaptixWidget->DownloadsDock->setCompactMode(d.FilesCompactMode);
    }
}

void MainUI::RebuildToolbars()
{
    const int pos = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.ToolbarPosition : 0;
    for (auto* adaptixWidget : AdaptixProjects) {
        if (adaptixWidget)
            adaptixWidget->rebuildToolbarLayout(pos);
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
    mainuiTabWidget->removeTab(currentIndex);
    delete adaptixWidget;
}

void MainUI::onCloseProjectRequested()
{
    QString projectName;
    if (auto* p = GetCurrentProfile())
        projectName = p->GetProject();
    QString text = projectName.isEmpty()
        ? QStringLiteral("Close the current project?")
        : QStringLiteral("Close project '%1'?").arg(projectName);
    auto reply = QMessageBox::question(this, "Close project", text,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes)
        onCloseProject();
}

void MainUI::onScriptsDock()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainuiTabWidget->currentWidget() );
    if (!adaptixWidget)
        return;

    adaptixWidget->LoadScriptsUI();
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

void MainUI::onTabContextMenu(const QPoint &pos)
{
    int index = mainuiTabWidget->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    mainuiTabWidget->setCurrentIndex(index);

    QMenu menu(this);
    menu.addAction("Subscriptions", this, &MainUI::onProjectSubscriptions);
    menu.addSeparator();
    menu.addAction("Close project", this, &MainUI::onCloseProjectRequested);
    menu.exec(mainuiTabWidget->tabBar()->mapToGlobal(pos));
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
        if (adaptixWidget && adaptixWidget->GetProfile()) {
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

    const AppTypography& ty = FontManager::instance().typography();
    const int tabH = ty.tabBarHeight;
    const qreal s = ty.baseSize / 10.0;

    auto* tabWidget = new QWidget();
    auto* layout = new QHBoxLayout(tabWidget);
    layout->setContentsMargins(showButton ? 12 : 16, 0, showButton ? 6 : 12, 0);
    layout->setSpacing(1);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    tabWidget->setFixedHeight(tabH);
    tabWidget->setMinimumHeight(tabH);
    tabWidget->setMaximumHeight(tabH);
    tabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* label = new QLabel(realName);
    label->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label->setFixedHeight(tabH);
    label->setMinimumHeight(tabH);
    label->setMaximumHeight(tabH);
    label->setFont(ty.regular);
    label->setProperty("transparent", true);

    layout->addWidget(label, 1, Qt::AlignCenter);

    if (showButton) {
        auto* folderButton = new QPushButton(tabWidget);
        folderButton->setIcon(QIcon(":/icons/folder"));

        const int iconSize = qMax(12, qRound(14 * s));
        const int buttonHeight = qMax(14, tabH - 8);
        const int buttonWidth = qMax(20, qRound(24 * s));

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