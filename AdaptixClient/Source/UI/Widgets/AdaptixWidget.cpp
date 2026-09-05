#include <Agent/Agent.h>
#include <Workers/LastTickWorker.h>
#include <Workers/WebSocketWorker.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/CustomElements/ConnectionStatusWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/ScriptsWidget.h>
#include <UI/Widgets/CodeEditorWidget.h>
#include <Client/CodeEditorProfileManager.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <UI/Widgets/SessionsFeedWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/ListenersFeedWidget.h>
#include <UI/Widgets/PayloadsFeedWidget.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <UI/Widgets/ScreenshotsFeedWidget.h>
#include <UI/Widgets/CredentialsFeedWidget.h>
#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/TargetsFeedWidget.h>
#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/TunnelsFeedWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogSyncPacket.h>
#include <UI/Dialogs/DialogTunnel.h>
#include <UI/Dialogs/DialogSettings.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxCommandWrappers.h>
#include <Client/DockLayoutEngine.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>
#include <Utils/FontManager.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/widgets/Popover.hpp>
#include <oclero/qlementine/widgets/Menu.hpp>

#include <QAction>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJSEngine>
#include <QPointer>
#include <QElapsedTimer>
#include <QTimer>
#include <QEvent>


AdaptixWidget::AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker)
{
    this->profile = authProfile;

    this->createUI();
    this->ChannelThread   = channelThread;
    this->ChannelWsWorker = channelWsWorker;

    pendingPacketsTimer = new QTimer(this);
    pendingPacketsTimer->setInterval(0);
    connect(pendingPacketsTimer, &QTimer::timeout, this, &AdaptixWidget::processPendingSyncPackets);

    ScriptManager = new AxScriptManager(this, this);

    connect(this, &AdaptixWidget::eventNewAgent,           ScriptManager, &AxScriptManager::emitNewAgent);
    connect(this, &AdaptixWidget::eventFileBrowserDisks,   ScriptManager, &AxScriptManager::emitFileBrowserDisks);
    connect(this, &AdaptixWidget::eventFileBrowserList,    ScriptManager, &AxScriptManager::emitFileBrowserList);
    connect(this, &AdaptixWidget::eventFileBrowserUpload,  ScriptManager, &AxScriptManager::emitFileBrowserUpload);
    connect(this, &AdaptixWidget::eventProcessBrowserList, ScriptManager, &AxScriptManager::emitProcessBrowserList);

    CodeEditorDock = new CodeEditorWidget(this);
    CodeEditorDock->connectConsoleSignals(ScriptManager);

    ScriptsDock       = new ScriptsWidget(this);
    LogsDock          = new LogsWidget(this);
    ChatDock          = new ChatWidget(this);
    ListenersDock     = new ListenersFeedWidget(this);
    PayloadsDock      = new PayloadsFeedWidget(this);
    SessionsGraphDock = new SessionsGraph(this);
    if (GlobalClient && GlobalClient->settings && GlobalClient->settings->data.SessionsViewMode == 1)
        SessionsTableDock = new SessionsFeedWidget(this);
    else
        SessionsTableDock = new SessionsTableWidget(this);
    TasksDock         = new TasksFeedWidget(this);
    TunnelsDock       = new TunnelsFeedWidget(this);
    DownloadsDock     = new FilesFeedWidget(this);
    ScreenshotsDock   = new ScreenshotsFeedWidget(this);
    if (GlobalClient && GlobalClient->settings && GlobalClient->settings->data.CredentialsViewMode == 0)
        CredentialsDock = new CredentialsWidget(this);
    else
        CredentialsDock = new CredentialsFeedWidget(this);
    if (GlobalClient && GlobalClient->settings && GlobalClient->settings->data.TargetsViewMode == 0)
        TargetsDock = new TargetsWidget(this);
    else
        TargetsDock = new TargetsFeedWidget(this);

    {
        DockLayoutSettings dockSettings = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.DockLayout : DockLayoutEngine::defaultsForLayout(QStringLiteral("main_right"));
        DockLayoutEngine::ensureValid(dockSettings);
        if (GlobalClient && GlobalClient->settings)
            GlobalClient->settings->data.DockLayout = dockSettings;

        layoutEngine.openStartup(this, dockSettings);
    }

    wireUnreadDocks();

    TickThread = new QThread;
    TickWorker = new LastTickWorker( this );
    TickWorker->moveToThread( TickThread );

    connect( this, &AdaptixWidget::SyncedSignal, this,   &AdaptixWidget::OnSynced);
    connect( this, &AdaptixWidget::SyncedSignal, ScriptManager, &AxScriptManager::emitReadyClient);

    connect( logsButton,          &QPushButton::clicked, this, &AdaptixWidget::LoadLogsUI);
    connect( chatButton,          &QPushButton::clicked, this, &AdaptixWidget::LoadChatUI);
    connect( listenersButton,     &QPushButton::clicked, this, &AdaptixWidget::LoadListenersUI);
    listenersButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( listenersButton,     &QPushButton::customContextMenuRequested, this, &AdaptixWidget::onListenersButtonContextMenu);
    connect( payloadsButton,      &QPushButton::clicked, this, &AdaptixWidget::LoadPayloadsUI);
    payloadsButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( payloadsButton,      &QPushButton::customContextMenuRequested, this, &AdaptixWidget::onPayloadsButtonContextMenu);
    connect( sessionsButton,      &QPushButton::clicked, this, &AdaptixWidget::SetSessionsTableUI);
    connect( graphButton,         &QPushButton::clicked, this, &AdaptixWidget::SetGraphUI);
    connect( tasksButton,         &QPushButton::clicked, this, &AdaptixWidget::SetTasksUI);
    connect( tunnelButton,        &QPushButton::clicked, this, &AdaptixWidget::LoadTunnelsUI);
    connect( downloadsButton,     &QPushButton::clicked, this, [this]() { LoadFilesUI(0); });
    downloadsButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( downloadsButton,     &QPushButton::customContextMenuRequested, this, &AdaptixWidget::onFilesButtonContextMenu);
    connect( screensButton,       &QPushButton::clicked, this, &AdaptixWidget::LoadScreenshotsUI);
    connect( credsButton,         &QPushButton::clicked, this, &AdaptixWidget::LoadCredentialsUI);
    connect( targetsButton,       &QPushButton::clicked, this, &AdaptixWidget::LoadTargetsUI);
    connect( connStatusWidget,    &QPushButton::clicked, this, &AdaptixWidget::OnReconnect);
    connect( scriptManagerButton, &QPushButton::clicked, this, [this]() {
        LoadScriptsUI(0, QStringLiteral("local"));
    });
    scriptManagerButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( scriptManagerButton, &QPushButton::customContextMenuRequested, this, &AdaptixWidget::onScriptsButtonContextMenu);
    connect( codeEditorButton,    &QPushButton::clicked, this, [this]() { LoadCodeEditorUI(); });
    connect( settingsButton,      &QPushButton::clicked, this, [](){ GlobalClient->settings->getDialogSettings()->show(); });

    connect( TickThread, &QThread::started, TickWorker, &LastTickWorker::run );
    connect( TickWorker, &LastTickWorker::agentTickUpdate, this, [this](const QList<AgentMarkInfo> &marks) {
        QList<qint64> agentIds;
        agentIds.reserve(marks.size());
        QReadLocker locker(&AgentsMapLock);
        for (const auto& m : marks) {
            if (!AgentsMap.contains(m.agentId)) continue;
            auto* agent = AgentsMap[m.agentId];
            agent->MarkItem(m.mark);
            agent->LastMark = m.lastMark;
            agentIds.append(m.agentId);
        }
        locker.unlock();
        SessionsTableDock->UpdateLastColumn(agentIds);
        for (qint64 id : agentIds)
            Q_EMIT agentTickUpdated(id);
    }, Qt::QueuedConnection);

    connect( ChannelWsWorker, &WebSocketWorker::received_json,    this,   &AdaptixWidget::DataHandlerJson );
    connect( ChannelWsWorker, &WebSocketWorker::received_data,    this,   &AdaptixWidget::DataHandler );
    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, this,   &AdaptixWidget::ChannelClose );
    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, ScriptManager, &AxScriptManager::emitDisconnectClient );

    dialogSyncPacket = new DialogSyncPacket(this);

    ChannelWsWorker->setHandlerReady();
    dialogSyncPacket->splashScreen->show();

    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, this, [this]() {
        if (this->sync && dialogSyncPacket) {
            dialogSyncPacket->error("Connection lost during synchronization");
            this->sync = false;
            this->syncFinishReceived = false;
            this->pendingPackets.clear();
            this->deferredTaskPackets.clear();
            this->deferredTransferPackets.clear();
            if (pendingPacketsTimer)
                pendingPacketsTimer->stop();
            this->setSyncUpdateUI(true);
        }
    });

    connect( dialogSyncPacket, &DialogSyncPacket::syncCancelled, this, [this]() {
        this->sync = false;
        this->syncFinishReceived = false;
        this->pendingPackets.clear();
        this->deferredTaskPackets.clear();
        this->deferredTransferPackets.clear();
        if (pendingPacketsTimer)
            pendingPacketsTimer->stop();
        this->setSyncUpdateUI(true);
        if (dialogSyncPacket && dialogSyncPacket->splashScreen)
            dialogSyncPacket->splashScreen->close();
    });

    TickThread->start();
    ChannelThread->start();

    QTimer::singleShot(100, this, [this]() {
        QByteArray jsonData = QJsonDocument(QJsonObject()).toJson();
        HttpRequestManager::instance().post(profile->GetURL(), "/sync", profile->GetAccessToken(), jsonData, [](bool, const QString&, const QJsonObject&) {});
    });
}

AdaptixWidget::~AdaptixWidget()
{
    Close();
    QWriteLocker locker(&AgentsMapLock);
    for (auto agent : AgentsMap.values()) {
        delete agent;
    }
    AgentsMap.clear();
}

void AdaptixWidget::finalizeSyncIfReady()
{
    if (!this->syncFinishReceived)
        return;
    if (!this->pendingPackets.isEmpty())
        return;

    this->syncFinishReceived = false;
    this->sync = false;

    if (dialogSyncPacket)
        dialogSyncPacket->finish();

    if (dialogSyncPacket) {
        dialogSyncPacket->setPhase("Applying UI updates...", true);
        if (dialogSyncPacket->splashScreen)
            dialogSyncPacket->splashScreen->repaint();
    }

    this->setSyncUpdateUI(true);

    if (dialogSyncPacket && dialogSyncPacket->splashScreen)
        dialogSyncPacket->splashScreen->close();

    Q_EMIT this->SyncedSignal();
}

void AdaptixWidget::enqueueSyncPacket(const QJsonObject &jsonObj)
{
    pendingPackets.enqueue(jsonObj);
    if (pendingPacketsTimer && !pendingPacketsTimer->isActive())
        pendingPacketsTimer->start();
}

void AdaptixWidget::processPendingSyncPackets()
{
    if (!pendingPacketsTimer)
        return;

    static bool processingPackets = false;
    if (processingPackets)
        return;
    processingPackets = true;

    QElapsedTimer timer;
    timer.start();

    int timeBudgetMs;
    if (this->sync) {
        timeBudgetMs = 50;
    } else {
        int queued = pendingPackets.size();
        if      (queued < 20)  timeBudgetMs = 6;
        else if (queued < 100) timeBudgetMs = 12;
        else                   timeBudgetMs = 30;
    }

    while (!pendingPackets.isEmpty()) {
        if (dialogSyncPacket && dialogSyncPacket->cancelled) {
            pendingPackets.clear();
            deferredTaskPackets.clear();
            deferredTransferPackets.clear();
            pendingPacketsTimer->stop();
            this->syncFinishReceived = false;
            this->syncTotalBatches = 0;
            this->syncProcessingBatchIndex = 0;
            this->syncProcessingBatchTotal = 0;
            this->syncProcessingBatchProcessed = 0;
            processingPackets = false;
            return;
        }

        QJsonObject obj = pendingPackets.dequeue();

        if (obj.contains("__ax_batch_marker") && obj.value("__ax_batch_marker").toBool()) {
            this->syncProcessingBatchIndex++;
            this->syncProcessingBatchTotal = obj.value("__ax_batch_size").toInt();
            this->syncProcessingBatchProcessed = 0;
            if (this->sync && dialogSyncPacket) {
                dialogSyncPacket->setProcessingProgress(
                    this->syncProcessingBatchIndex,
                    this->syncTotalBatches,
                    this->syncProcessingBatchProcessed,
                    this->syncProcessingBatchTotal
                );
            }

            this->syncProcessingUiTimer.restart();
        } else {
            this->processSyncPacket(obj);
            if (this->sync && this->syncProcessingBatchTotal > 0) {
                this->syncProcessingBatchProcessed++;
                bool shouldUpdate = false;
                if (!this->syncProcessingUiTimer.isValid()) {
                    shouldUpdate = true;
                    this->syncProcessingUiTimer.start();
                } else if (this->syncProcessingUiTimer.elapsed() >= 150) {
                    shouldUpdate = true;
                    this->syncProcessingUiTimer.restart();
                } else if (this->syncProcessingBatchProcessed >= this->syncProcessingBatchTotal) {
                    shouldUpdate = true;
                }

                if (shouldUpdate && dialogSyncPacket) {
                    dialogSyncPacket->setProcessingProgress(
                        this->syncProcessingBatchIndex,
                        this->syncTotalBatches,
                        this->syncProcessingBatchProcessed,
                        this->syncProcessingBatchTotal
                    );
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
            }
        }

        if (timer.elapsed() >= timeBudgetMs)
            break;
    }

    if (pendingPackets.isEmpty())
        pendingPacketsTimer->stop();

    finalizeSyncIfReady();
    processingPackets = false;
}

void AdaptixWidget::createUI()
{
    createButtons();

    extDocksListWidget = new QListWidget();
    extDocksListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    extDocksListWidget->setFrameShape(QFrame::NoFrame);

    extDocksEmptyLabel = new QLabel("No loaded extenders docks");
    extDocksEmptyLabel->setAlignment(Qt::AlignCenter);

    auto* extDocksContent = new QWidget();
    auto* extDocksLayout = new QVBoxLayout(extDocksContent);
    extDocksLayout->setContentsMargins(4, 4, 4, 4);
    extDocksLayout->setSpacing(4);
    extDocksLayout->addWidget(extDocksListWidget);
    extDocksLayout->addWidget(extDocksEmptyLabel);
    extDocksContent->setMinimumWidth(220);

    extDocksPopover = new oclero::qlementine::Popover(extDocksButton);
    extDocksPopover->setAnchorWidget(extDocksButton);
    extDocksPopover->setPreferredPosition(oclero::qlementine::Popover::Position::Bottom);
    extDocksPopover->setPreferredAlignment(oclero::qlementine::Popover::Alignment::Begin);
    auto* popQs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    if (popQs) {
        const auto& pt = popQs->theme();
        extDocksPopover->setBackgroundColor(pt.backgroundColorMain2);
        extDocksPopover->setBorderColor(pt.borderColor);
        extDocksPopover->setBorderWidth(1.0);
        extDocksPopover->setRadius(6.0);
        extDocksPopover->setPadding(QMargins(4, 4, 4, 4));
        const QString popupTextCss = QStringLiteral(
            "QListWidget { background: %1; color: palette(text); }"
            "QLabel     { color: palette(text); }"
        ).arg(pt.backgroundColorMain1.name());
        extDocksListWidget->setStyleSheet(popupTextCss);
        extDocksEmptyLabel->setStyleSheet(popupTextCss);
    }
    extDocksPopover->setContentWidget(extDocksContent);

    connect(extDocksButton, &QPushButton::clicked, this, [this]() {
        if (extDocksPopover)
            extDocksPopover->openPopover();
    });
    connect(extDocksListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString id = item->data(Qt::UserRole).toString();
        if (extDocksMap.contains(id) && extDocksMap[id].showCallback) {
            extDocksMap[id].showCallback();
        }
        if (extDocksPopover)
            extDocksPopover->setOpened(false);
    });

    const int pos = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.ToolbarPosition : 0;
    const bool vertical = (pos == 2 || pos == 3);

    buildSegmentedGroups(vertical);

    buildToolbarLayout(pos);

    mainDockWidget = new KDDockWidgets::QtWidgets::MainWindow(this->profile->GetProject()+"-MainDock", KDDockWidgets::MainWindowOption_None);
    layoutEngine.attach(mainDockWidget, this->profile->GetProject());
    {
        DockLayoutSettings dockSettings = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.DockLayout : DockLayoutEngine::defaultsForLayout(QStringLiteral("split_v2"));
        DockLayoutEngine::ensureValid(dockSettings);
        layoutEngine.build(dockSettings);
    }

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->setHorizontalSpacing(0);

    placeToolbarInGrid(mainGridLayout, pos);
    mainGridLayout->addWidget(mainDockWidget, 1, 1, 1, 1);

    this->setLayout(mainGridLayout);

    applyThemeColorsToToolbar();

    if (auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr)) {
        connect(qs, &oclero::qlementine::QlementineStyle::themeChanged, this, &AdaptixWidget::applyThemeColorsToToolbar, Qt::UniqueConnection);
    }
    QTimer::singleShot(0, this, [this]() { applyThemeColorsToToolbar(); });

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        if (!mainGridLayout || !toolbarWidget)
            return;
        const int pos = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.ToolbarPosition : 0;
        placeToolbarInGrid(mainGridLayout, pos);
        const AppTypography& ty = FontManager::instance().typography();
        const qreal s = ty.baseSize / 10.0;
        const int icon = qMax(18, qRound(24 * s));
        const int bw = qMax(28, qRound(37 * s));
        const int bh = ty.controlHeight;
        for (auto* btn : toolbarWidget->findChildren<QPushButton*>()) {
            if (btn == settingsButton)
                continue;
            if (!btn->icon().isNull() && btn->text().isEmpty()) {
                btn->setIconSize(QSize(icon, icon));
                btn->setFixedSize(bw, bh);
            }
        }
    });
}


void AdaptixWidget::createButtons()
{
    auto mkIconBtn = [this](const QString& iconPath, const QString& tooltip) {
        const AppTypography& ty = FontManager::instance().typography();
        const qreal s = ty.baseSize / 10.0;
        const int icon = qMax(18, qRound(24 * s));
        const int bw = qMax(28, qRound(37 * s));
        const int bh = ty.controlHeight;
        auto* btn = new QPushButton(QIcon(iconPath), "", this);
        btn->setIconSize(QSize(icon, icon));
        btn->setFixedSize(bw, bh);
        btn->setToolTip(tooltip);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    sessionsButton = mkIconBtn(":/icons/format_list", "Session table");
    graphButton    = mkIconBtn(":/icons/graph",       "Session graph");
    tasksButton    = mkIconBtn(":/icons/job",         "Jobs & Tasks");

    listenersButton = mkIconBtn(":/icons/listeners", "Listeners & Sites (right-click: Create Listener)");
    payloadsButton  = mkIconBtn(":/icons/kill",      "Payload Store (right-click: Generate)");
    tunnelButton    = mkIconBtn(":/icons/vpn",       "Tunnels table");
    logsButton      = mkIconBtn(":/icons/logs",      "Notifications");

    downloadsButton = mkIconBtn(":/icons/downloads", "Files (right-click: Downloads / Uploads / Sync)");
    targetsButton   = mkIconBtn(":/icons/devices",   "Targets table");
    credsButton     = mkIconBtn(":/icons/key",       "Credentials");
    screensButton   = mkIconBtn(":/icons/picture",   "Screens");
    keysButton      = mkIconBtn(":/icons/keyboard",  "Keystrokes");
    chatButton      = mkIconBtn(":/icons/chat",      "Chat");

    scriptManagerButton = mkIconBtn(":/icons/folder_code", "Scripts (right-click: Local / Teamserver / Events)");
    codeEditorButton    = mkIconBtn(":/icons/code",        "Code editor");
    extDocksButton      = mkIconBtn(":/icons/extension",   "Extensions Docks");

    settingsButton = mkIconBtn(":/icons/settings_account", "Settings");

    connStatusWidget = new ConnectionStatusWidget(this);
    connStatusWidget->setState(ConnectionStatusWidget::Connected);
    connStatusWidget->setFocusPolicy(Qt::NoFocus);

    /// TODO: Enable menu button
    keysButton->setVisible(false);
}


void AdaptixWidget::buildSegmentedGroups(bool vertical)
{
    auto applyGroupStyle = [vertical](QFrame* frame) {
        frame->setObjectName("SegmentedGroup");
        for (auto* child : frame->findChildren<QPushButton*>()) {
            if (vertical) {
                child->setIconSize(QSize(28, 28));
                child->setFixedSize(48, 40);
            } else {
                child->setIconSize(QSize(24, 24));
                child->setFixedSize(37, 28);
            }        }
    };

    auto buildGroup = [&](QFrame*& member, const QList<QWidget*>& widgets, const QList<int>& separatedIndices) {
        member = new QFrame(this);
        if (vertical)
            member->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
        member->setLayout(vertical ? static_cast<QLayout*>(new QVBoxLayout()) : static_cast<QLayout*>(new QHBoxLayout()));
        member->layout()->setContentsMargins(2, 2, 2, 2);
        member->layout()->setSpacing(1);
        const auto childAlign = vertical ? Qt::AlignHCenter : Qt::Alignment();
        auto* boxLayout = qobject_cast<QBoxLayout*>(member->layout());
        for (int i = 0; i < widgets.size(); ++i) {
            if (separatedIndices.contains(i)) {
                auto* sep = new QLabel();
                if (vertical)
                    sep->setText(QStringLiteral("---"));
                else
                    sep->setText(QStringLiteral("│"));
                sep->setAlignment(Qt::AlignCenter);
                sep->setObjectName("ToolbarSeparator");
                if (boxLayout) boxLayout->addWidget(sep, 0, childAlign);
                else member->layout()->addWidget(sep);
            }
            if (boxLayout) boxLayout->addWidget(widgets[i], 0, childAlign);
            else member->layout()->addWidget(widgets[i]);
        }
        applyGroupStyle(member);
    };

    // View: Sessions / Graph | Tasks
    buildGroup(groupView, { sessionsButton, graphButton, tasksButton }, {2});
    // Infra: Listeners / Payload Store | Logs / Chat | Tunnels
    buildGroup(groupInfra, { listenersButton, payloadsButton, logsButton, chatButton, tunnelButton }, {2, 4});
    // Data: Downloads / Targets / Creds / Screens / Keys
    buildGroup(groupData, { downloadsButton, targetsButton, credsButton, screensButton, keysButton }, {});
    // Dev: Script manager / Code editor | Extensions
    buildGroup(groupDev, { scriptManagerButton, codeEditorButton, extDocksButton }, {2});
}

void AdaptixWidget::applyThemeColorsToToolbar()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    if (toolbarWidget) {
        const QColor rowBg = t.backgroundColorMain2.isValid() ? t.backgroundColorMain2
                             : (t.backgroundColorTabBar.isValid() ? t.backgroundColorTabBar
                                                                  : t.backgroundColorMain1);
        QPalette pal = toolbarWidget->palette();
        for (auto g : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            pal.setColor(g, QPalette::Window, rowBg);
            pal.setColor(g, QPalette::Base, rowBg);
            pal.setColor(g, QPalette::Button, rowBg);
        }
        toolbarWidget->setPalette(pal);
        toolbarWidget->setAutoFillBackground(true);
        toolbarWidget->setStyleSheet(QStringLiteral(
            "QWidget#ToolbarContainer {"
            "  background-color: %1;"
            "  border: none;"
            "}"
        ).arg(rowBg.name(QColor::HexRgb)));
    }

    const QColor groupBg = t.neutralColor.isValid() ? t.neutralColor : t.backgroundColorMain3;
    const QColor groupBrd = t.borderColor.isValid() ? t.borderColor : t.borderColorDisabled;

    struct GroupPainter : QObject {
        QColor bg, brd;
        qreal rad;
        GroupPainter(const QColor& b, const QColor& br, qreal r, QObject* p = nullptr) : QObject(p), bg(b), brd(br), rad(r) {
            p->installEventFilter(this);
        }
        void setColors(const QColor& b, const QColor& br) { bg = b; brd = br; }
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (ev->type() == QEvent::Paint) {
                auto* frame = qobject_cast<QFrame*>(obj);
                if (!frame) return false;
                QPainter p(frame);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(QPen(brd, 1));
                p.setBrush(bg);
                p.drawRoundedRect(QRectF(frame->rect()).adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
                return false;
            }
            return false;
        }
    };
    if (!m_groupPainter) {
        m_groupPainter = new GroupPainter(groupBg, groupBrd, 5.0, this);
        for (auto* f : { groupView, groupInfra, groupData, groupDev })
            if (f) f->installEventFilter(m_groupPainter);
    } else {
        static_cast<GroupPainter*>(m_groupPainter)->setColors(groupBg, groupBrd);
        for (auto* f : { groupView, groupInfra, groupData, groupDev })
            if (f) f->update();
    }

    if (extDocksPopover && qs) {
        extDocksPopover->setBackgroundColor(t.backgroundColorMain2);
        extDocksPopover->setBorderColor(t.borderColor);
        if (extDocksListWidget && extDocksEmptyLabel) {
            const QString popupTextCss = QStringLiteral(
                "QListWidget { background: %1; color: palette(text); }"
                "QLabel     { color: palette(text); }"
            ).arg(t.backgroundColorMain1.name());
            extDocksListWidget->setStyleSheet(popupTextCss);
            extDocksEmptyLabel->setStyleSheet(popupTextCss);
        }
    }
}

void AdaptixWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        if (toolbarWidget)
            applyThemeColorsToToolbar();
    }
}

void AdaptixWidget::buildToolbarLayout(int position)
{
    const bool vertical = (position == 2 || position == 3);

    if (toolbarWidget) {
        for (auto* member : { static_cast<QWidget*>(settingsButton), static_cast<QWidget*>(connStatusWidget) })
            if (member) member->setParent(this);
        for (auto* frame : { groupView, groupInfra, groupData, groupDev })
            if (frame) frame->setParent(this);
        delete toolbarWidget;
        toolbarWidget = nullptr;
    }

    toolbarWidget = new QWidget(this);
    toolbarWidget->setObjectName("ToolbarContainer");

    auto* box = vertical ? static_cast<QBoxLayout*>(new QVBoxLayout(toolbarWidget))
                         : static_cast<QBoxLayout*>(new QHBoxLayout(toolbarWidget));
    toolbarLayout = box;

    box->setContentsMargins(vertical ? 6 : 6, vertical ? 8 : 4, vertical ? 6 : 6, vertical ? 8 : 4);
    box->setSpacing(vertical ? 10 : 8);
    const int groupGap = vertical ? 6 : 6;

    connStatusWidget->setCompact(vertical);

    if (vertical) {
        settingsButton->setIconSize(QSize(28, 28));
        settingsButton->setFixedSize(48, 40);
    } else {
        settingsButton->setIconSize(QSize(24, 24));
        settingsButton->setFixedSize(37, 28);
    }

    const auto vAlign = vertical ? Qt::AlignHCenter : Qt::Alignment();
    box->addWidget(groupView,  0, vAlign);
    box->addSpacing(groupGap);
    box->addWidget(groupInfra, 0, vAlign);
    box->addSpacing(groupGap);
    box->addWidget(groupData,  0, vAlign);
    box->addSpacing(groupGap);
    box->addWidget(groupDev,   0, vAlign);

    auto* spacerItem = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->addItem(spacerItem);
    box->addWidget(connStatusWidget, 0, vAlign);
    box->addWidget(settingsButton,   0, vAlign);
}

void AdaptixWidget::placeToolbarInGrid(QGridLayout* grid, int position)
{
    if (!grid || !toolbarWidget)
        return;

    const AppTypography& ty = FontManager::instance().typography();
    const int barH = ty.mainToolbarH;
    const int sideW = ty.sideToolbarW;

    switch (position) {
    case 1:
        toolbarWidget->setFixedHeight(barH);
        grid->addWidget(toolbarWidget, 2, 1, 1, 1);
        break;
    case 2:
        toolbarWidget->setFixedWidth(sideW);
        toolbarWidget->layout()->setContentsMargins(6, 8, 14, 8);
        grid->addWidget(toolbarWidget, 1, 0, 1, 1);
        break;
    case 3:
        toolbarWidget->setFixedWidth(sideW);
        toolbarWidget->layout()->setContentsMargins(14, 8, 6, 8);
        grid->addWidget(toolbarWidget, 1, 2, 1, 1);
        break;
    case 0:
    default:
        toolbarWidget->setFixedHeight(barH);
        toolbarWidget->layout()->setContentsMargins(6, 4, 6, 4);
        grid->addWidget(toolbarWidget, 0, 1, 1, 1);
        break;
    }
}

void AdaptixWidget::rebuildToolbarLayout(int position)
{
    if (!mainGridLayout || !toolbarWidget)
        return;

    mainGridLayout->removeWidget(toolbarWidget);

    toolbarWidget->setFixedHeight(QWIDGETSIZE_MAX);
    toolbarWidget->setFixedWidth(QWIDGETSIZE_MAX);
    toolbarWidget->updateGeometry();

    QFrame* oldFrames[4] = { groupView, groupInfra, groupData, groupDev };

    if (m_groupPainter) {
        for (QFrame* f : oldFrames)
            if (f) f->removeEventFilter(m_groupPainter);
        delete m_groupPainter;
        m_groupPainter = nullptr;
    }

    const bool vertical = (position == 2 || position == 3);
    buildSegmentedGroups(vertical);
    for (QFrame* f : oldFrames) {
        if (f)
            f->deleteLater();
    }

    buildToolbarLayout(position);
    placeToolbarInGrid(mainGridLayout, position);
    applyThemeColorsToToolbar();
}

/// MAIN

AuthProfile* AdaptixWidget::GetProfile() const { return this->profile; }

void AdaptixWidget::PlaceDock(KDDockWidgets::QtWidgets::DockWidget* parentDock, KDDockWidgets::QtWidgets::DockWidget* dock) const
{
    if (!parentDock || !dock)
        return;

    if (dock->isOpen()) {
        dock->setAsCurrentTab();
        return;
    }

    QString previousFocusedName;
    const QString dockBeingAddedName = dock->uniqueName();

    if (KDDockWidgets::DockRegistry::self()) {
        if (auto* previousFocused = KDDockWidgets::DockRegistry::self()->focusedDockWidget())
            previousFocusedName = previousFocused->uniqueName();
    }

    if (!parentDock->isOpen())
        parentDock->open();

    if (!parentDock->group())
        parentDock->open();

    if (parentDock->group()) {
        parentDock->addDockWidgetAsTab(dock);
        const bool parentIsZoneHost = parentDock->uniqueName().contains(QLatin1String("-Zone-"));
        if (parentIsZoneHost && parentDock->group() && parentDock->group()->dockWidgets().size() > 1)
            parentDock->close();
    } else {
        dock->open();
    }

    if (!previousFocusedName.isEmpty() && previousFocusedName != dockBeingAddedName) {
        QTimer::singleShot(100, [previousFocusedName, dockBeingAddedName]() {
            if (!KDDockWidgets::DockRegistry::self())
                return;
            auto* currentFocused = KDDockWidgets::DockRegistry::self()->focusedDockWidget();
            if (currentFocused && currentFocused->uniqueName() == dockBeingAddedName)
                return;
            if (currentFocused && currentFocused->uniqueName() != previousFocusedName
                && currentFocused->uniqueName() != dockBeingAddedName)
                return;
            auto* coreDw = KDDockWidgets::DockRegistry::self()->dockByName(previousFocusedName);
            if (coreDw && !coreDw->isCurrentTab())
                coreDw->setAsCurrentTab();
        });
    }
}

void AdaptixWidget::PlaceWidget(const QString& widgetId, KDDockWidgets::QtWidgets::DockWidget* dock, const QString& zoneOverride) const
{
    layoutEngine.placeWidget(widgetId, dock, const_cast<AdaptixWidget*>(this), zoneOverride);
}

bool AdaptixWidget::AddExtension(ExtensionFile* ext)
{
    if( !synchronized ) {
        ext->Enabled = false;
        ext->Message = "C2 not synchronized";
        return false;
    }

    return ScriptManager->ScriptAdd(ext);
}

void AdaptixWidget::RemoveExtension(const ExtensionFile &ext)
{
    if (!ScriptManager->ScriptList().contains(ext.FilePath))
        return;

    return ScriptManager->ScriptRemove(ext);
}

bool AdaptixWidget::IsSynchronized() const { return this->synchronized; }

namespace {
void stopHostedWorker(QObject* worker, QThread* host)
{
    if (worker) {
        const bool crossThread = host && host->isRunning() && worker->thread() == host && QThread::currentThread() != host;
        QMetaObject::invokeMethod(worker, "stopWorker", crossThread ? Qt::QueuedConnection : Qt::DirectConnection);
    }
    if (host && host->isRunning()) {
        host->quit();
        if (!host->wait(3000)) {
            host->terminate();
            host->wait(1000);
        }
    }
}
}

void AdaptixWidget::Close()
{
    if (m_closed)
        return;
    m_closed = true;

    if (TickWorker)
        disconnect(TickWorker, nullptr, this, nullptr);
    stopHostedWorker(TickWorker, TickThread);
    TickWorker = nullptr;
    TickThread = nullptr;

    if (ChannelWsWorker) {
        disconnect(ChannelWsWorker, nullptr, this, nullptr);
        if (ScriptManager)
            disconnect(ChannelWsWorker, nullptr, ScriptManager, nullptr);
    }
    stopHostedWorker(ChannelWsWorker, ChannelThread);
    ChannelWsWorker = nullptr;
    ChannelThread = nullptr;

    this->ClearAdaptix();

    LogsDock->deleteLater();
    ChatDock->deleteLater();
    ListenersDock->deleteLater();
    if (PayloadsDock) {
        PayloadsDock->deleteLater();
        PayloadsDock = nullptr;
    }
    SessionsGraphDock->deleteLater();
    if (TasksDock) {
        TasksDock->deleteLater();
        TasksDock = nullptr;
    }
    TunnelsDock->deleteLater();
    DownloadsDock->deleteLater();
    ScreenshotsDock->deleteLater();
    if (CredentialsDock) {
        if (auto* w = CredentialsDock->asWidget())
            w->deleteLater();
        CredentialsDock = nullptr;
    }
    if (TargetsDock) {
        if (auto* w = TargetsDock->asWidget())
            w->deleteLater();
        TargetsDock = nullptr;
    }
    if (SessionsTableDock) {
        if (auto* w = SessionsTableDock->asWidget())
            w->deleteLater();
        SessionsTableDock = nullptr;
    }

    for (auto* host : layoutEngine.allHosts()) {
        if (host)
            host->deleteLater();
    }
    layoutEngine.clear();
    if (mainDockWidget)
        mainDockWidget->deleteLater();

    delete dialogSyncPacket;
    dialogSyncPacket = nullptr;

    delete profile;
    profile = nullptr;
}

void AdaptixWidget::ClearAdaptix()
{
    synchronized = false;
    clearAllDockUnread();

    LogsDock->Clear();
    ChatDock->Clear();
    DownloadsDock->Clear();
    ScreenshotsDock->Clear();
    TasksDock->Clear();
    ListenersDock->Clear();
    if (PayloadsDock)
        PayloadsDock->Clear();
    SessionsGraphDock->Clear();
    SessionsTableDock->Clear();
    TunnelsDock->Clear();
    CredentialsDock->Clear();
    TargetsDock->Clear();

    {
        QWriteLocker locker(&AgentsMapLock);
        for (auto agent : AgentsMap.values()) {
            SessionsGraphDock->RemoveAgent(agent, false);
            SessionsTableDock->RemoveAgentItem(agent->data.Id);
            TasksDock->RemoveAgentTasksItem(agent->data.Id);
            delete agent;
        }
        AgentsMap.clear();
    }

    for (auto tunnelId : ClientTunnels.keys()) {
        auto tunnel = ClientTunnels[tunnelId];
        ClientTunnels.remove(tunnelId);
        tunnel->Stop();
        delete tunnel;
    }
    ClientTunnels.clear();
    GraphTunnelMarks.clear();

    ScriptManager->Clear();

    for (auto regAgent : RegisterAgents)
        delete regAgent.commander;
    RegisterAgents.clear();

    RegisterListeners.clear();
    AgentTypes.clear();
    Listeners.clear();
}

void AdaptixWidget::ClearChatStream()
{
    if (ChatDock)
        ChatDock->Clear();
    ChatUnreadClear();
}

static void ensureToolbarBadge(oclero::qlementine::NotificationBadge*& badge, QWidget* host)
{
    if (!host)
        return;
    if (!badge) {
        QWidget* parent = host->parentWidget() ? host->parentWidget() : host;
        badge = new oclero::qlementine::NotificationBadge(parent);
        badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        badge->setAttribute(Qt::WA_NoSystemBackground);
        badge->setPadding(QMargins(3, 1, 3, 1));
        badge->setRelativePosition(2, -2);
        badge->setWidget(host);
        badge->hide();
    } else if (badge->widget() != host) {
        badge->setWidget(host);
    }
}

static void setToolbarBadgeCount(oclero::qlementine::NotificationBadge* badge, int count)
{
    if (!badge)
        return;
    if (count <= 0) {
        badge->setText(QString());
        badge->hide();
        return;
    }
    badge->setText(count > 99 ? QStringLiteral("99+") : QString::number(count));
    badge->show();
    badge->raise();
    QTimer::singleShot(0, badge, [badge]() {
        if (!badge || badge->text().isEmpty())
            return;
        badge->resize(badge->sizeHint());
        badge->show();
        badge->raise();
    });
}

static bool dockIsViewed(KDDockWidgets::QtWidgets::DockWidget* dock)
{
    if (!dock)
        return false;
    auto* core = dock->dockWidget();
    return core && core->isOpen() && core->isCurrentTab();
}

void AdaptixWidget::setUnreadCount(UnreadKind kind, int count)
{
    const int idx = static_cast<int>(kind);
    if (idx < 0 || idx >= static_cast<int>(UnreadKind::Count))
        return;
    if (count < 0)
        count = 0;
    m_unreadCounts[idx] = count;

    oclero::qlementine::NotificationBadge** slot = nullptr;
    QPushButton* button = nullptr;
    switch (kind) {
        case UnreadKind::Sessions:  slot = &sessionsBadge;  button = sessionsButton;  break;
        case UnreadKind::Listeners: slot = &listenersBadge; button = listenersButton; break;
        case UnreadKind::Logs:      slot = &logsBadge;      button = logsButton;      break;
        case UnreadKind::Chat:      slot = &chatBadge;      button = chatButton;      break;
        case UnreadKind::Downloads: slot = &downloadsBadge; button = downloadsButton; break;
        case UnreadKind::Creds:     slot = &credsBadge;     button = credsButton;     break;
        case UnreadKind::Targets:   slot = &targetsBadge;   button = targetsButton;   break;
        case UnreadKind::Screens:   slot = &screensBadge;   button = screensButton;   break;
        case UnreadKind::Count:     break;
    }
    if (!slot)
        return;
    if (count > 0)
        ensureToolbarBadge(*slot, button);
    setToolbarBadgeCount(*slot, count);
}

bool AdaptixWidget::isUnreadDockViewed(UnreadKind kind) const
{
    switch (kind) {
        case UnreadKind::Sessions:
            return SessionsTableDock && dockIsViewed(SessionsTableDock->dock());
        case UnreadKind::Listeners:
            return ListenersDock && dockIsViewed(ListenersDock->dock());
        case UnreadKind::Logs:
            return LogsDock && dockIsViewed(LogsDock->dock());
        case UnreadKind::Chat:
            return ChatDock && dockIsViewed(ChatDock->dock());
        case UnreadKind::Downloads:
            return DownloadsDock && dockIsViewed(DownloadsDock->dock()) && DownloadsDock->currentSegment() == 0;
        case UnreadKind::Creds:
            return CredentialsDock && dockIsViewed(CredentialsDock->dock());
        case UnreadKind::Targets:
            return TargetsDock && dockIsViewed(TargetsDock->dock());
        case UnreadKind::Screens:
            return ScreenshotsDock && dockIsViewed(ScreenshotsDock->dock());
        case UnreadKind::Count:
            break;
    }
    return false;
}

void AdaptixWidget::notifyDockUnread(UnreadKind kind, int count)
{
    if (!synchronized || count <= 0)
        return;
    if (isUnreadDockViewed(kind))
        return;
    const int idx = static_cast<int>(kind);
    if (idx < 0 || idx >= static_cast<int>(UnreadKind::Count))
        return;
    setUnreadCount(kind, m_unreadCounts[idx] + count);
}

void AdaptixWidget::clearDockUnread(UnreadKind kind)
{
    setUnreadCount(kind, 0);
}

void AdaptixWidget::clearAllDockUnread()
{
    for (int i = 0; i < static_cast<int>(UnreadKind::Count); ++i)
        setUnreadCount(static_cast<UnreadKind>(i), 0);
}

void AdaptixWidget::wireUnreadDocks()
{
    auto wire = [this](KDDockWidgets::QtWidgets::DockWidget* dock, UnreadKind kind) {
        if (!dock)
            return;
        connect(dock, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, [this, kind](bool current) {
            if (!current)
                return;
            if (kind == UnreadKind::Downloads && DownloadsDock && DownloadsDock->currentSegment() != 0)
                return;
            clearDockUnread(kind);
        });
        connect(dock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [this, dock, kind](bool open) {
            if (!open)
                return;
            auto* core = dock->dockWidget();
            if (!core || !core->isCurrentTab())
                return;
            if (kind == UnreadKind::Downloads && DownloadsDock && DownloadsDock->currentSegment() != 0)
                return;
            clearDockUnread(kind);
        });
    };

    if (SessionsTableDock)
        wire(SessionsTableDock->dock(), UnreadKind::Sessions);
    if (ListenersDock)
        wire(ListenersDock->dock(), UnreadKind::Listeners);
    if (LogsDock)
        wire(LogsDock->dock(), UnreadKind::Logs);
    if (ChatDock)
        wire(ChatDock->dock(), UnreadKind::Chat);
    if (DownloadsDock)
        wire(DownloadsDock->dock(), UnreadKind::Downloads);
    if (CredentialsDock)
        wire(CredentialsDock->dock(), UnreadKind::Creds);
    if (TargetsDock)
        wire(TargetsDock->dock(), UnreadKind::Targets);
    if (ScreenshotsDock)
        wire(ScreenshotsDock->dock(), UnreadKind::Screens);
}

void AdaptixWidget::ChatUnreadIncrement()
{
    notifyDockUnread(UnreadKind::Chat);
}

void AdaptixWidget::ChatUnreadClear()
{
    clearDockUnread(UnreadKind::Chat);
}

void AdaptixWidget::LogsUnreadIncrement()
{
    notifyDockUnread(UnreadKind::Logs);
}

void AdaptixWidget::LogsUnreadClear()
{
    clearDockUnread(UnreadKind::Logs);
}

void AdaptixWidget::ClearConsoleStreams()
{
    QReadLocker locker(&AgentsMapLock);
    for (const auto agent : AgentsMap.values()) {
        if (agent && agent->Console)
            agent->Console->Clear();
    }
}

void AdaptixWidget::ClearNotificationsStream()
{
    if (LogsDock)
        LogsDock->Clear();
    LogsUnreadClear();
}

/// REGISTER

void AdaptixWidget::RegisterListenerConfig(const QString &name, const QString &protocol, const QString &type, const QString &ax_script)
{
    ScriptManager->ListenerScriptAdd(name, ax_script);
    RegListenerConfig config = { name, protocol, type };
    RegisterListeners.push_back(config);
}

void AdaptixWidget::RegisterServiceConfig(const QString &serviceName, const QString &ax_script)
{
    ScriptManager->ServiceScriptAdd(serviceName, ax_script);
}

static Argument parseArgument(const QJsonObject &argObj)
{
    Argument arg;
    arg.type         = argObj["type"].toString();
    arg.name         = argObj["name"].toString();
    arg.required     = argObj["required"].toBool();
    arg.flag         = argObj["flag"].toBool();
    arg.mark         = argObj["mark"].toString();
    arg.description  = argObj["description"].toString();
    arg.defaultUsed  = argObj["default_used"].toBool();
    if (arg.defaultUsed)
        arg.defaultValue = argObj["default_value"].toVariant();
    return arg;
}

static Command parseCommand(const QJsonObject &cmdObj)
{
    Command cmd;
    cmd.name        = cmdObj["name"].toString();
    cmd.message     = cmdObj["message"].toString();
    cmd.description = cmdObj["description"].toString();
    cmd.example     = cmdObj["example"].toString();
    cmd.is_pre_hook = cmdObj["has_pre_hook"].toBool();

    for (const QJsonValue &argVal : cmdObj["args"].toArray()) {
        if (argVal.isObject())
            cmd.args.append(parseArgument(argVal.toObject()));
    }

    for (const QJsonValue &subVal : cmdObj["subcommands"].toArray()) {
        if (subVal.isObject())
            cmd.subcommands.append(parseCommand(subVal.toObject()));
    }
    return cmd;
}

static CommandsGroup parseCommandsGroup(const QString &scriptName, const QJsonArray &cmdsArray)
{
    QList<Command> commands;
    for (const QJsonValue &cmdVal : cmdsArray) {
        if (cmdVal.isObject())
            commands.append(parseCommand(cmdVal.toObject()));
    }

    CommandsGroup cg;
    cg.groupName = scriptName;
    cg.commands  = commands;
    cg.engine    = nullptr;
    cg.filepath  = QStringLiteral("__server__:") + scriptName;
    return cg;
}

void AdaptixWidget::RegisterAgentConfig(const QString &agentName, const QString &ax_script, const QStringList &listeners, const bool &multiListeners, const QJsonArray &groups)
{
    AgentTypes[agentName] = AgentTypeInfo{multiListeners, listeners};

    ScriptManager->AgentScriptAdd(agentName, ax_script);

    for (const auto &listener : listeners) {
        for (int os : {OS_WINDOWS, OS_LINUX, OS_MAC}) {
            Commander* commander = nullptr;
            for (auto &regAgent : this->RegisterAgents) {
                if (regAgent.name == agentName && regAgent.listenerType == listener && regAgent.os == os) {
                    commander = regAgent.commander;
                    break;
                }
            }
            if (!commander) {
                commander = new Commander();
                commander->SetAgentType(agentName);
                RegisterAgents.push_back({agentName, listener, os, commander, true});
            } else {
                commander->ClearMainGroups();
            }
        }
    }

    {
        QReadLocker locker(&AgentsMapLock);
        for (auto* agent : AgentsMap) {
            if (agent && agent->commander && agent->data.Name == agentName)
                agent->commander->ClearMainGroups();
        }
    }

    QJSEngine* engine = ScriptManager->AgentScriptEngine(agentName);

    for (const QJsonValue &groupVal : groups) {
        if (!groupVal.isObject())
            continue;

        QJsonObject groupObj = groupVal.toObject();
        QString gAgent = groupObj["agent"].toString();
        QString gListener = groupObj["listener"].toString();
        int gOs = static_cast<int>(groupObj["os"].toDouble());
        QString commandsJson = groupObj["commands"].toString();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(commandsJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray())
            continue;

        QJsonArray commandGroupsArray = doc.array();
        for (const QJsonValue &cgVal : commandGroupsArray) {
            if (!cgVal.isObject())
                continue;

            QJsonObject cgObj = cgVal.toObject();
            QString groupName = cgObj["groupName"].toString();
            if (groupName.isEmpty())
                groupName = cgObj["group_name"].toString();
            QString groupDesc = cgObj["groupDescription"].toString();
            if (groupDesc.isEmpty())
                groupDesc = cgObj["group_description"].toString();
            QJsonArray cmdsArray = cgObj["commands"].toArray();
            bool defaultEnabled = true;
            if (cgObj.contains(QStringLiteral("default_enabled")))
                defaultEnabled = cgObj["default_enabled"].toBool(true);
            else if (cgObj.contains(QStringLiteral("defaultEnabled")))
                defaultEnabled = cgObj["defaultEnabled"].toBool(true);

            if (groupName.isEmpty())
                groupName = agentName;

            CommandsGroup cg = parseCommandsGroup(groupName, cmdsArray);
            if (cg.commands.isEmpty())
                continue;

            cg.engine = engine;

            for (auto &regAgent : this->RegisterAgents) {
                if (regAgent.name != gAgent || regAgent.os != gOs)
                    continue;
                bool listenerMatch = gListener.isEmpty() || regAgent.listenerType.isEmpty() || regAgent.listenerType == gListener;
                if (listenerMatch) {
                    regAgent.commander->AddMainGroup(cg, groupDesc, defaultEnabled);
                }
            }

            {
                QReadLocker locker(&AgentsMapLock);
                for (auto* agent : AgentsMap) {
                    if (!agent || !agent->commander)
                        continue;
                    if (agent->data.Name != gAgent || agent->data.Os != gOs)
                        continue;
                    bool lMatch = gListener.isEmpty() || agent->listenerType.isEmpty() || agent->listenerType == gListener;
                    if (lMatch)
                        agent->commander->AddMainGroup(cg, groupDesc, defaultEnabled);
                }
            }
        }
    }
}

void AdaptixWidget::registerServerCommandGroups(const QString &scriptName, const QList<ServerScriptGroup> &groups, QJSEngine* engine)
{
    for (const auto &group : groups) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(group.commandsJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray())
            continue;

        for (const QJsonValue &groupVal : doc.array()) {
            if (!groupVal.isObject())
                continue;

            QJsonObject groupObj = groupVal.toObject();
            QString groupName = groupObj["groupName"].toString();
            if (groupName.isEmpty())
                groupName = groupObj["group_name"].toString();
            QString groupDesc = groupObj["groupDescription"].toString();
            if (groupDesc.isEmpty())
                groupDesc = groupObj["group_description"].toString();
            bool defaultEnabled = true;
            if (groupObj.contains(QStringLiteral("default_enabled")))
                defaultEnabled = groupObj["default_enabled"].toBool(true);
            else if (groupObj.contains(QStringLiteral("defaultEnabled")))
                defaultEnabled = groupObj["defaultEnabled"].toBool(true);

            if (groupName.isEmpty())
                groupName = scriptName;

            CommandsGroup cg = parseCommandsGroup(groupName, groupObj["commands"].toArray());
            if (cg.commands.isEmpty())
                continue;

            cg.filepath = QStringLiteral("__server__:") + scriptName;
            cg.engine = engine;

            for (auto &regAgent : this->RegisterAgents) {
                if (regAgent.name != group.agentName || regAgent.os != group.os)
                    continue;
                bool listenerMatch = group.listenerType.isEmpty() || regAgent.listenerType.isEmpty() || regAgent.listenerType == group.listenerType;
                if (listenerMatch)
                    regAgent.commander->AddServerGroup(groupName, groupDesc, cg, defaultEnabled);
            }

            QReadLocker locker(&AgentsMapLock);
            for (auto agent : AgentsMap) {
                if (agent->data.Name != group.agentName || agent->data.Os != group.os)
                    continue;
                bool lMatch = group.listenerType.isEmpty() || agent->listenerType.isEmpty() || agent->listenerType == group.listenerType;
                if (lMatch && agent->commander)
                    agent->commander->AddServerGroup(groupName, groupDesc, cg, defaultEnabled);
            }
        }
    }

    if (engine) {
        for (auto &regAgent : this->RegisterAgents)
            regAgent.commander->SetServerGroupEngine(scriptName, engine);

        QReadLocker locker(&AgentsMapLock);
        for (auto agent : AgentsMap)
            agent->commander->SetServerGroupEngine(scriptName, engine);
    }
}

void AdaptixWidget::ProcessAxScriptPacket(const QString &name, const QString &content, const QJsonArray &groups)
{
    ServerScriptData scriptData;
    scriptData.name = name;
    scriptData.code = content;
    scriptData.enabled = true;

    for (const QJsonValue &groupVal : groups) {
        if (!groupVal.isObject())
            continue;

        QJsonObject groupObj = groupVal.toObject();
        ServerScriptGroup sg;
        sg.agentName    = groupObj["agent"].toString();
        sg.listenerType = groupObj["listener"].toString();
        sg.os           = static_cast<int>(groupObj["os"].toDouble());
        sg.commandsJson = groupObj["commands"].toString();
        scriptData.groups.append(sg);
    }

    ScriptManager->ServerScriptAdd(scriptData);
    registerServerCommandGroups(name, scriptData.groups, ScriptManager->ServerScriptEngine(name));

    Q_EMIT serverScriptsChanged();
}

void AdaptixWidget::EnableServerScript(const QString &name)
{
    ServerScriptData data = ScriptManager->ServerScriptGet(name);
    if (data.name.isEmpty())
        return;

    ScriptManager->ServerScriptSetEnabled(name, true);
    registerServerCommandGroups(name, data.groups, ScriptManager->ServerScriptEngine(name));

    Q_EMIT serverScriptsChanged();
}

void AdaptixWidget::DisableServerScript(const QString &name)
{
    ScriptManager->ServerScriptSetEnabled(name, false);

    for (auto &regAgent : this->RegisterAgents)
        regAgent.commander->RemoveServerGroup(name);

    QReadLocker locker(&AgentsMapLock);
    for (auto agent : AgentsMap)
        agent->commander->RemoveServerGroup(name);

    Q_EMIT serverScriptsChanged();
}

QList<ServerScriptInfo> AdaptixWidget::GetServerScripts() const
{
    QList<ServerScriptInfo> result;
    if (!ScriptManager)
        return result;

    for (const auto &data : ScriptManager->ServerScriptList()) {
        ServerScriptInfo info;
        info.name = data.name;
        info.description = data.description;
        info.enabled = data.enabled;
        result.append(info);
    }
    return result;
}

RegListenerConfig AdaptixWidget::GetRegListener(const QString &listenerName)
{
    for (auto regListener : this->RegisterListeners)
        if (regListener.name == listenerName)
            return regListener;

    return RegListenerConfig{};
}

QList<QString> AdaptixWidget::GetAgentNames(const QString &listenerType) const
{
    QSet<QString> names;
    for (auto it = AgentTypes.constBegin(); it != AgentTypes.constEnd(); ++it) {
        if (it.value().listenerTypes.contains(listenerType))
            names.insert(it.key());
    }
    return names.values();
}

AgentTypeInfo AdaptixWidget::GetAgentTypeInfo(const QString &agentName) const
{
    return AgentTypes.value(agentName, AgentTypeInfo{false, QStringList()});
}

RegAgentConfig AdaptixWidget::GetRegAgent(const QString &agentName, const QString &listenerName, const int os)
{
    if (os == OS_WINDOWS || os == OS_LINUX || os == OS_MAC) {
        QString listener = "";
        for ( auto listenerData : this->Listeners) {
            if ( listenerData.Name == listenerName ) {
                listener = listenerData.ListenerRegName;
                break;
            }
        }
        for (auto regAgent : this->RegisterAgents) {
            if (regAgent.name == agentName && regAgent.listenerType == listener && regAgent.os == os)
                return regAgent;
        }
        for (auto regAgent : this->RegisterAgents) {
            if (regAgent.name == agentName && regAgent.listenerType.isEmpty() && regAgent.os == os)
                return regAgent;
        }
        for (auto regAgent : this->RegisterAgents) {
            if (regAgent.name == agentName && regAgent.os == os)
                return regAgent;
        }
    }
    return {};
}

QList<Commander*> AdaptixWidget::GetCommanders(const QStringList &listeners, const QStringList &agents, const QList<int> &os) const
{
    QList<Commander*> commanders;
    auto matches = [&](const QString &name, const QString &listenerType, int agentOs) {
        if (!agents.contains(name))
            return false;
        if (!listeners.empty() && !listenerType.isEmpty() && !listeners.contains(listenerType))
            return false;
        if (!os.empty() && !os.contains(agentOs))
            return false;
        return true;
    };

    for (const auto &regAgent : this->RegisterAgents) {
        if (!regAgent.commander || !matches(regAgent.name, regAgent.listenerType, regAgent.os))
            continue;
        if (!commanders.contains(regAgent.commander))
            commanders.append(regAgent.commander);
    }

    QReadLocker locker(&AgentsMapLock);
    for (auto agent : AgentsMap) {
        if (!agent || !agent->commander)
            continue;
        if (!matches(agent->data.Name, agent->listenerType, agent->data.Os))
            continue;
        if (!commanders.contains(agent->commander))
            commanders.append(agent->commander);
    }
    return commanders;
}

QList<Commander*> AdaptixWidget::GetCommandersAll() const
{
    QList<Commander*> commanders;
    for (const auto &regAgent : this->RegisterAgents) {
        if (regAgent.commander && !commanders.contains(regAgent.commander))
            commanders.append(regAgent.commander);
    }
    QReadLocker locker(&AgentsMapLock);
    for (auto agent : AgentsMap) {
        if (agent && agent->commander && !commanders.contains(agent->commander))
            commanders.append(agent->commander);
    }
    return commanders;
}

void AdaptixWidget::AddCommandsToCommanders(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &osList)
{
    QList<int> effectiveOs = osList.isEmpty() ? QList<int>{OS_WINDOWS, OS_LINUX, OS_MAC} : osList;

    for (const QString &agentName : agents) {
        for (int os : effectiveOs) {
            if (listeners.isEmpty()) {
                bool found = false;
                for (auto &regAgent : this->RegisterAgents) {
                    if (regAgent.name == agentName && regAgent.os == os) {
                        regAgent.commander->AddClientGroup(group);
                        found = true;
                    }
                }
                if (!found) {
                    auto* targetCommander = new Commander();
                    targetCommander->SetAgentType(agentName);
                    RegAgentConfig config = {agentName, "", os, targetCommander, true};
                    RegisterAgents.push_back(config);
                    targetCommander->AddClientGroup(group);
                }

                QReadLocker locker(&AgentsMapLock);
                for (auto agent : AgentsMap) {
                    if (agent->data.Name == agentName && agent->data.Os == os)
                        agent->commander->AddClientGroup(group);
                }
            } else {
                for (const QString &listener : listeners) {
                    Commander* targetCommander = nullptr;

                    for (auto &regAgent : this->RegisterAgents) {
                        if (regAgent.name == agentName && regAgent.os == os) {
                            bool listenerMatch = regAgent.listenerType.isEmpty() || regAgent.listenerType == listener;
                            if (listenerMatch) {
                                targetCommander = regAgent.commander;
                                break;
                            }
                        }
                    }

                    if (!targetCommander) {
                        targetCommander = new Commander();
                        targetCommander->SetAgentType(agentName);
                        RegAgentConfig config = {agentName, listener, os, targetCommander, true};
                        RegisterAgents.push_back(config);
                    }

                    targetCommander->AddClientGroup(group);
                }

                QReadLocker locker(&AgentsMapLock);
                for (auto agent : AgentsMap) {
                    if (agent->data.Name != agentName || agent->data.Os != os)
                        continue;
                    bool listenerMatch = listeners.isEmpty() || agent->listenerType.isEmpty();
                    if (!listenerMatch) {
                        for (const QString &listener : listeners) {
                            if (agent->listenerType == listener) {
                                listenerMatch = true;
                                break;
                            }
                        }
                    }
                    if (listenerMatch)
                        agent->commander->AddClientGroup(group);
                }
            }
        }
    }
}

void AdaptixWidget::PostHookProcess(QJsonObject jsonHookObj)
{
    QString hookId = jsonHookObj["a_hook_id"].toString();
    bool completed = jsonHookObj["a_completed"].toBool();

    AxExecutor post_hooks;
    bool hookFound = false;
    {
        QWriteLocker locker(&PostHooksLock);
        if (PostHooksJS.contains(hookId)) {
            post_hooks = PostHooksJS[hookId];
            hookFound = true;
            if (completed)
                PostHooksJS.remove(hookId);
        }
    }
    if (hookFound) {

        auto jsEngine = ScriptManager->GetEngine(post_hooks.engineName);
        if (jsEngine && post_hooks.executor.isCallable()) {

            int jobIndex = jsonHookObj["a_job_index"].toDouble();

            QJsonObject obj;
            obj["agent"]     = jsonHookObj["a_id"].toString();;
            obj["message"]   = jsonHookObj["a_message"].toString();
            obj["text"]      = jsonHookObj["a_text"].toString();
            obj["completed"] = completed;
            obj["index"]     = jobIndex;

            int msgType = jsonHookObj["a_msg_type"].toDouble();
            if (msgType == CONSOLE_OUT_LOCAL_INFO || msgType == CONSOLE_OUT_INFO)
                obj["type"] = "info";
            else if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR)
                obj["type"] = "error";
            else if (msgType == CONSOLE_OUT_LOCAL_SUCCESS || msgType == CONSOLE_OUT_SUCCESS)
                obj["type"] = "success";
            else
                obj["type"] = "";

            QJSValue result = post_hooks.executor.call(QJSValueList() << jsEngine->toScriptValue(obj));
            if (result.isObject()) {
                QJsonObject modifiedObj = result.toVariant().toJsonObject();

                if (modifiedObj.contains("message") && modifiedObj["message"].isString())
                    jsonHookObj["a_message"] = modifiedObj["message"].toString();
                if (modifiedObj.contains("text") && modifiedObj["text"].isString())
                    jsonHookObj["a_text"] = modifiedObj["text"].toString();
                if (modifiedObj.contains("type") && modifiedObj["type"].isString()) {
                    QString modifiedType = modifiedObj["type"].toString();
                    if (modifiedType == "info")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_INFO;
                    else if (modifiedType == "error")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_ERROR;
                    else if (modifiedType == "success")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_SUCCESS;
                    else
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT;
                }
            }
        }
    }

    QByteArray jsonData = QJsonDocument(jsonHookObj).toJson();

    HttpReqTasksHookAsync(jsonData, *profile, [](bool success, const QString &message, const QJsonObject&) {
        if (!success)
            MessageError(message);
    });
}

void AdaptixWidget::PostHandlerProcess(const QString &handlerId, const TaskData &taskData)
{
    AxExecutor post_handler;
    bool handlerFound = false;
    {
        QWriteLocker locker(&PostHandlersLock);
        if (PostHandlersJS.contains(handlerId)) {
            post_handler = PostHandlersJS.take(handlerId);
            handlerFound = true;
        }
    }
    if (handlerFound) {
        auto jsEngine = ScriptManager->GetEngine(post_handler.engineName);
        if (jsEngine && post_handler.executor.isCallable()) {

            QJsonObject obj;
            obj["id"]      = taskData.TaskId;
            obj["agent"]   = taskData.AgentId;
            obj["cmdline"] = taskData.CommandLine;
            obj["message"] = taskData.Message;
            obj["text"]    = taskData.Output;

            int msgType = taskData.MessageType;
            if (msgType == CONSOLE_OUT_LOCAL_INFO || msgType == CONSOLE_OUT_INFO)
                obj["type"] = "info";
            else if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR)
                obj["type"] = "error";
            else if (msgType == CONSOLE_OUT_LOCAL_SUCCESS || msgType == CONSOLE_OUT_SUCCESS)
                obj["type"] = "success";
            else
                obj["type"] = "";

            post_handler.executor.call(QJSValueList() << jsEngine->toScriptValue(obj));
        }
    }
}

/// SHOW PANELS

void AdaptixWidget::LoadConsoleUI(qint64 AgentId)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->Console) {
        locker.unlock();
        this->PlaceWidget(QStringLiteral("console"), agent->Console->dock());
        agent->Console->LoadInitialPage();
        agent->Console->InputFocus();
    }
}

void AdaptixWidget::LoadTasksOutput() const
{
    if (!mainDockWidget || !TasksDock)
        return;

    auto* tasksDock  = TasksDock->dockTasks();
    auto* outputDock = TasksDock->dockTasksOutput();
    if (!tasksDock || !outputDock)
        return;

    SetTasksUI();

    if (outputDock->isOpen()) {
        outputDock->open();
        return;
    }

    if (tasksDock->isOpen() && mainDockWidget) {
        mainDockWidget->addDockWidget(outputDock, KDDockWidgets::Location_OnBottom, tasksDock);
        TasksOutputDockPlaced = true;
    } else {
        this->PlaceWidget(QStringLiteral("tasks_output"), outputDock);
        TasksOutputDockPlaced = true;
    }
}

void AdaptixWidget::LoadFileBrowserUI(qint64 AgentId, const QString& zoneOverride)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceWidget(QStringLiteral("files"), agent->GetFileBrowser()->dock(), zoneOverride);
}

void AdaptixWidget::LoadProcessBrowserUI(qint64 AgentId, const QString& zoneOverride)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceWidget(QStringLiteral("processes"), agent->GetProcessBrowser()->dock(), zoneOverride);
}

void AdaptixWidget::LoadTerminalUI(qint64 AgentId, const QString& zoneOverride)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceWidget(QStringLiteral("terminal"), agent->GetTerminal()->dock(), zoneOverride);
}

static void applyCodeEditorOpenPayload(CodeEditorWidget* editor, const CodeEditorOpenOptions& opts)
{
    if (!editor)
        return;

    if (!opts.panelSeed.isEmpty() && !opts.profile.isEmpty()) {
        if (auto* mgr = CodeEditorProfileManager::instance()) {
            if (const BuildProfile* base = mgr->profile(opts.profile)) {
                BuildProfile p = *base;
                for (auto it = opts.panelSeed.begin(); it != opts.panelSeed.end(); ++it)
                    p.panelState.insert(it.key(), it.value());
                mgr->updateProfile(p);
            }
        }
    }

    editor->applyOpenOptions(opts);
    if (!opts.restrictProfiles && opts.profile.isEmpty())
        editor->selectProfile(QStringLiteral("system.axscript"));
    else if (!opts.profile.isEmpty())
        editor->selectProfile(opts.profile);

    if (!opts.filePath.isEmpty())
        editor->openScript(opts.filePath);
    else if (!opts.contentName.isEmpty() || !opts.content.isEmpty() || !opts.documentKey.isEmpty())
        editor->openScriptContent(
            opts.contentName.isEmpty() ? QStringLiteral("untitled") : opts.contentName,
            opts.content,
            opts.documentKey);

    if (!opts.profile.isEmpty())
        editor->selectProfile(opts.profile);
}

void AdaptixWidget::LoadAgentCodeEditorUI(qint64 AgentId, const CodeEditorOpenOptions& opts)
{
    QReadLocker locker(&AgentsMapLock);
    if (!AgentsMap.contains(AgentId))
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (!agent)
        return;

    CodeEditorWidget* editor = agent->GetCodeEditor();
    if (!editor)
        return;

    applyCodeEditorOpenPayload(editor, opts);
    this->PlaceWidget(QStringLiteral("code_editor"), editor->dock(), opts.zone);
}

void AdaptixWidget::LoadShellUI(qint64 AgentId, const QString& zoneOverride)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceWidget(QStringLiteral("shell"), agent->GetShell()->dock(), zoneOverride);
}

void AdaptixWidget::ShowTunnelCreator(qint64 AgentId, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd)
{
    DialogTunnel* dialogTunnel = new DialogTunnel(AgentId, socks4, socks5, lportfwd, rportfwd);

    while (true) {
        dialogTunnel->StartDialog();
        if (dialogTunnel->IsValid())
            break;

        QString msg = dialogTunnel->GetMessage();
        if (msg.isEmpty()) {
            delete dialogTunnel;
            return;
        }

        MessageError(msg);
    }

    QString    tunnelType = dialogTunnel->GetTunnelType();
    QString    endpoint   = dialogTunnel->GetEndpoint();
    QByteArray tunnelData = dialogTunnel->GetTunnelData();

    if ( endpoint == "Teamserver" ) {
        HttpReqTunnelStartServerAsync(tunnelType, tunnelData, *profile, [dialogTunnel](bool success, const QString &message, const QJsonObject&) {
            if (!success)
                MessageError(message);
            delete dialogTunnel;
        });
    }
    else {
        auto tunnelEndpoint = new TunnelEndpoint();
        bool started = tunnelEndpoint->StartTunnel(profile, tunnelType, tunnelData);
        if (started) {
            QPointer<AdaptixWidget> safeThis = this;
            HttpReqTunnelStartServerAsync(tunnelType, tunnelData, *profile, [safeThis, tunnelEndpoint, dialogTunnel](bool success, const QString &message, const QJsonObject&) {
                if (!success) {
                    MessageError(message);
                    delete tunnelEndpoint;
                } else {
                    qint64 tunnelId = message.toLongLong();
                    tunnelEndpoint->SetTunnelId(tunnelId);
                    if (safeThis) {
                        safeThis->ClientTunnels[tunnelId] = tunnelEndpoint;
                    } else {
                        tunnelEndpoint->Stop();
                        delete tunnelEndpoint;
                    }
                    MessageSuccess("Tunnel #" + QString::number(tunnelId) + " started");
                }
                delete dialogTunnel;
            });
        }
        else {
            delete tunnelEndpoint;
            delete dialogTunnel;
        }
    }
}

/// SLOTS

void AdaptixWidget::ChannelClose() const
{
    if (!ChannelThread) return;
    if (connStatusWidget)
        const_cast<ConnectionStatusWidget*>(connStatusWidget)->setState(ConnectionStatusWidget::Disconnected);
    ChannelThread->quit();
}

void AdaptixWidget::DataHandler(const QByteArray &data)
{
    LogError("Unexpected non-JSON websocket payload (len=%d).", static_cast<int>(data.size()));
}

void AdaptixWidget::DataHandlerJson(const QJsonObject &jsonObj)
{
    if (!this->isValidSyncPacket(jsonObj)) {
        QString msg = "Invalid SyncPacket";
        if (jsonObj.contains("type") && jsonObj["type"].isDouble()) {
            int spType = jsonObj["type"].toDouble();
            msg.append(": 0x" + QString::number(spType, 16).toUpper() + " (" + QString::number(spType) + ")");
        }
        LogError(msg.toStdString().c_str());
        return;
    }

    this->enqueueSyncPacket(jsonObj);
}

void AdaptixWidget::OnWebSocketConnected()
{
    QTimer::singleShot(100, this, [this]() {
        QByteArray jsonData = QJsonDocument(QJsonObject()).toJson();
        HttpRequestManager::instance().post(profile->GetURL(), "/sync", profile->GetAccessToken(), jsonData, [](bool, const QString&, const QJsonObject&) {});
    });
}

void AdaptixWidget::OnSynced()
{
    synchronized = true;

    QTimer::singleShot(0, this, [this]() {
        this->SessionsGraphDock->ApplyFiltersAndLayout();
        this->TasksDock->UpdateColumnsSize();
        this->TasksDock->UpdateFilterComboBoxes();
        this->SessionsTableDock->UpdateColumnsSize();
        this->SessionsTableDock->UpdateAgentTypeComboBox();
        this->CredentialsDock->UpdateColumnsSize();
        this->CredentialsDock->UpdateFilterComboBoxes();
        this->TargetsDock->UpdateColumnsSize();

        Q_EMIT SyncedOnReloadSignal(profile->GetProject());
    });
}

void AdaptixWidget::SetSessionsTableUI() const
{
    if (SessionsTableDock)
        this->PlaceWidget(QStringLiteral("sessions"), SessionsTableDock->dock());
    const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Sessions);
}

void AdaptixWidget::SetGraphUI() const
{
    if (SessionsGraphDock)
        this->PlaceWidget(QStringLiteral("graph"), SessionsGraphDock->dock());
}

void AdaptixWidget::SetTasksUI() const
{
    if (!TasksDock)
        return;

    auto* tasksDock = TasksDock->dockTasks();
    if (!tasksDock)
        return;

    if (tasksDock->isOpen()) {
        tasksDock->setAsCurrentTab();
        return;
    }

    auto* codeDock = (CodeEditorDock && CodeEditorDockPlaced) ? CodeEditorDock->dock() : nullptr;
    if (codeDock && codeDock->isOpen()) {
        codeDock->addDockWidgetAsTab(tasksDock);
        TasksDockPlaced = true;
        tasksDock->setAsCurrentTab();
        return;
    }

    this->PlaceWidget(QStringLiteral("tasks"), tasksDock);
    TasksDockPlaced = true;
    tasksDock->setAsCurrentTab();
}

void AdaptixWidget::LoadScriptsUI(int segment, const QString& originFilter) const
{
    if (ScriptsDock)
        ScriptsDock->setSegment(segment, originFilter, /*applyOriginFilter=*/true);
    if (ScriptsDock)
        this->PlaceWidget(QStringLiteral("scripts"), ScriptsDock->dock());
}

void AdaptixWidget::LoadCodeEditorUI(const CodeEditorOpenOptions& opts) const
{
    if (!mainDockWidget || !CodeEditorDock)
        return;

    applyCodeEditorOpenPayload(CodeEditorDock, opts);

    auto* dock = CodeEditorDock->dock();

    if (!CodeEditorDockPlaced) {
        CodeEditorDockPlaced = true;
        if (opts.zone.isEmpty()) {
            auto* tasksDock = (TasksDock && TasksDockPlaced) ? TasksDock->dockTasks() : nullptr;
            if (tasksDock && tasksDock->isOpen()) {
                tasksDock->addDockWidgetAsTab(dock);
                dock->setAsCurrentTab();
                return;
            }
        }
        this->PlaceWidget(QStringLiteral("code_editor"), dock, opts.zone);
        dock->setAsCurrentTab();
        return;
    }

    if (!dock->isOpen())
        dock->open();
    dock->setAsCurrentTab();
}

void AdaptixWidget::LoadLogsUI() const
{
    if (LogsDock)
        this->PlaceWidget(QStringLiteral("logs"), LogsDock->dock());
    const_cast<AdaptixWidget*>(this)->LogsUnreadClear();
}

void AdaptixWidget::LoadChatUI() const {
    if (ChatDock)
        this->PlaceWidget(QStringLiteral("chat"), ChatDock->dock());
    const_cast<AdaptixWidget*>(this)->ChatUnreadClear();
}

void AdaptixWidget::LoadListenersUI() const
{
    if (ListenersDock)
        this->PlaceWidget(QStringLiteral("listeners"), ListenersDock->dock());
    const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Listeners);
}

void AdaptixWidget::LoadPayloadsUI() const
{
    if (PayloadsDock) {
        this->PlaceWidget(QStringLiteral("payloads"), PayloadsDock->dock());
        PayloadsDock->SetUpdatesEnabled(true);
    }
}

void AdaptixWidget::LoadTunnelsUI() const
{
    if (TunnelsDock)
        this->PlaceWidget(QStringLiteral("tunnels"), TunnelsDock->dock());
}

void AdaptixWidget::LoadDownloadsUI() const { LoadFilesUI(0); }

void AdaptixWidget::LoadFilesUI(int segment) const
{
    if (DownloadsDock)
        DownloadsDock->setSegment(segment);
    if (DownloadsDock)
        this->PlaceWidget(QStringLiteral("downloads"), DownloadsDock->dock());
    if (segment == 0)
        const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Downloads);
}

void AdaptixWidget::onListenersButtonContextMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu(this);

    auto* actCreate = menu.addAction(QIcon(":/icons/plus"), "Create Listener");
    connect(actCreate, &QAction::triggered, this, [this]() {
        LoadListenersUI();
        if (ListenersDock)
            ListenersDock->onCreateListener();
    });

    if (listenersButton)
        menu.exec(listenersButton->mapToGlobal(pos));
}

void AdaptixWidget::onPayloadsButtonContextMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu(this);

    auto* actGenerate = menu.addAction(QIcon(":/icons/plus"), "Generate");
    connect(actGenerate, &QAction::triggered, this, [this]() {
        LoadPayloadsUI();
        if (PayloadsDock)
            PayloadsDock->onGenerateFromToolbar();
    });

    if (payloadsButton)
        menu.exec(payloadsButton->mapToGlobal(pos));
}

void AdaptixWidget::onFilesButtonContextMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu(this);

    auto* actDl = menu.addAction(QIcon(":/icons/arrow_cool_down"), "Downloads");
    connect(actDl, &QAction::triggered, this, [this]() { LoadFilesUI(0); });

    auto* actUl = menu.addAction(QIcon(":/icons/arrow_warm_up"), "Uploads");
    connect(actUl, &QAction::triggered, this, [this]() { LoadFilesUI(1); });

    auto* actSync = menu.addAction(QIcon(":/icons/data_arrows"), "Sync");
    connect(actSync, &QAction::triggered, this, [this]() { LoadFilesUI(2); });

    if (downloadsButton)
        menu.exec(downloadsButton->mapToGlobal(pos));
}

void AdaptixWidget::onScriptsButtonContextMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu(this);

    auto* actLocal = menu.addAction(QIcon(":/icons/computer"), "Scripts Local");
    connect(actLocal, &QAction::triggered, this, [this]() {
        LoadScriptsUI(0, QStringLiteral("local"));
    });

    auto* actServer = menu.addAction(QIcon(":/icons/storage"), "Scripts Teamserver");
    connect(actServer, &QAction::triggered, this, [this]() {
        LoadScriptsUI(0, QStringLiteral("server"));
    });

    menu.addSeparator();

    auto* actEvents = menu.addAction(QIcon(":/icons/calendar"), "Events");
    connect(actEvents, &QAction::triggered, this, [this]() { LoadScriptsUI(1); });

    if (scriptManagerButton)
        menu.exec(scriptManagerButton->mapToGlobal(pos));
}

void AdaptixWidget::LoadScreenshotsUI() const
{
    if (ScreenshotsDock)
        this->PlaceWidget(QStringLiteral("screenshots"), ScreenshotsDock->dock());
    const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Screens);
}

void AdaptixWidget::LoadCredentialsUI() const
{
    if (CredentialsDock)
        this->PlaceWidget(QStringLiteral("credentials"), CredentialsDock->dock());
    const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Creds);
}

void AdaptixWidget::LoadTargetsUI() const
{
    if (TargetsDock)
        this->PlaceWidget(QStringLiteral("targets"), TargetsDock->dock());
    const_cast<AdaptixWidget*>(this)->clearDockUnread(UnreadKind::Targets);
}

void AdaptixWidget::OnReconnect()
{
    if (connStatusWidget)
        connStatusWidget->setState(ConnectionStatusWidget::Reconnecting);

    if (ChannelThread->isRunning()) {
        QThread* workerThread = new QThread();
        QObject* worker = new QObject();
        worker->moveToThread(workerThread);

        QPointer<AdaptixWidget> safeThis = this;
        connect(workerThread, &QThread::started, worker, [=, this]() {
            bool result = HttpReqJwtUpdate(profile);

            QMetaObject::invokeMethod(qApp, [=, this]() {
                if (!safeThis) {
                    workerThread->quit();
                    workerThread->wait();
                    worker->deleteLater();
                    workerThread->deleteLater();
                    return;
                }

                if (!result) {
                    MessageError("Login failure");
                    if (connStatusWidget)
                        connStatusWidget->setState(ConnectionStatusWidget::Disconnected);
                } else {
                    if (connStatusWidget)
                        connStatusWidget->setState(ConnectionStatusWidget::Connected);
                }

                workerThread->quit();
                workerThread->wait();
                worker->deleteLater();
                workerThread->deleteLater();
            }, Qt::QueuedConnection);
        });

        workerThread->start();
    }
    else {
        QThread* workerThread = new QThread();
        QObject* worker = new QObject();
        worker->moveToThread(workerThread);

        QPointer<AdaptixWidget> safeThis = this;
        connect(workerThread, &QThread::started, worker, [=, this]() {
            bool result = HttpReqLogin(profile);

            QMetaObject::invokeMethod(qApp, [=, this]() {
                if (!safeThis) {
                    workerThread->quit();
                    workerThread->wait();
                    worker->deleteLater();
                    workerThread->deleteLater();
                    return;
                }

                if (!result) {
                    MessageError("Login failure");
                    if (dialogSyncPacket && dialogSyncPacket->splashScreen)
                        dialogSyncPacket->splashScreen->close();
                    if (connStatusWidget)
                        connStatusWidget->setState(ConnectionStatusWidget::Disconnected);
                } else {
                    this->ClearAdaptix();

                    connect( ChannelWsWorker, &WebSocketWorker::connected, this, &AdaptixWidget::OnWebSocketConnected, Qt::UniqueConnection );

                    ChannelThread->start();

                    if (connStatusWidget)
                        connStatusWidget->setState(ConnectionStatusWidget::Connected);
                }

                workerThread->quit();
                workerThread->wait();
                worker->deleteLater();
                workerThread->deleteLater();
            }, Qt::QueuedConnection);
        });

        if (dialogSyncPacket && dialogSyncPacket->splashScreen)
            dialogSyncPacket->splashScreen->show();

        workerThread->start();
    }
}

void AdaptixWidget::AddExtDock(const QString &id, const QString &title, const std::function<void()> &showCallback)
{
    if (extDocksMap.contains(id))
        return;

    ExtDockEntry entry;
    entry.id = id;
    entry.title = title;
    entry.showCallback = showCallback;
    extDocksMap[id] = entry;

    auto* item = new QListWidgetItem(title);
    item->setData(Qt::UserRole, id);
    extDocksListWidget->addItem(item);

    extDocksListWidget->setVisible(true);
    extDocksEmptyLabel->setVisible(false);
}

void AdaptixWidget::SetExtDockIcon(const QString &id, const QIcon &icon)
{
    for (int i = 0; i < extDocksListWidget->count(); ++i) {
        auto* item = extDocksListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == id) {
            item->setIcon(icon);
            break;
        }
    }
}

void AdaptixWidget::RemoveExtDock(const QString &id)
{
    if (!extDocksMap.contains(id))
        return;

    extDocksMap.remove(id);

    for (int i = 0; i < extDocksListWidget->count(); ++i) {
        auto* item = extDocksListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == id) {
            delete extDocksListWidget->takeItem(i);
            break;
        }
    }

    bool empty = extDocksListWidget->count() == 0;
    extDocksListWidget->setVisible(!empty);
    extDocksEmptyLabel->setVisible(empty);
}

void AdaptixWidget::ShowExtDocksPopup()
{
    if (!extDocksButton || !extDocksPopover)
        return;

    bool empty = extDocksListWidget->count() == 0;
    extDocksListWidget->setVisible(!empty);
    extDocksEmptyLabel->setVisible(empty);

    extDocksPopover->openPopover();
}
