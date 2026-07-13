#include <QJSEngine>
#include <QPointer>
#include <QElapsedTimer>
#include <QTimer>
#include <QEvent>
#include <Agent/Agent.h>
#include <Workers/LastTickWorker.h>
#include <Workers/WebSocketWorker.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/CustomElements/ConnectionStatusWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/ScriptsWidget.h>
#include <UI/Widgets/CodeEditorWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <UI/Widgets/SessionsFeedWidget.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/ListenersFeedWidget.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <UI/Widgets/ScreenshotsFeedWidget.h>
#include <UI/Widgets/CredentialsFeedWidget.h>
#include <UI/Widgets/TargetsFeedWidget.h>
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
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/widgets/Popover.hpp>

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

    if (CodeEditorDock)
        CodeEditorDock->connectConsoleSignals(ScriptManager);

    connect(this, &AdaptixWidget::eventNewAgent,           ScriptManager, &AxScriptManager::emitNewAgent);
    connect(this, &AdaptixWidget::eventFileBrowserDisks,   ScriptManager, &AxScriptManager::emitFileBrowserDisks);
    connect(this, &AdaptixWidget::eventFileBrowserList,    ScriptManager, &AxScriptManager::emitFileBrowserList);
    connect(this, &AdaptixWidget::eventFileBrowserUpload,  ScriptManager, &AxScriptManager::emitFileBrowserUpload);
    connect(this, &AdaptixWidget::eventProcessBrowserList, ScriptManager, &AxScriptManager::emitProcessBrowserList);

    CodeEditorDock = new CodeEditorWidget(this);

    ScriptsDock       = new ScriptsWidget(this);
    LogsDock          = new LogsWidget(this);
    ChatDock          = new ChatWidget(this);
    ListenersDock     = new ListenersFeedWidget(this);
    SessionsGraphDock = new SessionsGraph(this);
    SessionsTableDock = new SessionsFeedWidget(this);
    TasksDock         = new TasksFeedWidget(this);
    TunnelsDock       = new TunnelsFeedWidget(this);
    DownloadsDock     = new FilesFeedWidget(this);
    ScreenshotsDock   = new ScreenshotsFeedWidget(this);
    CredentialsDock   = new CredentialsFeedWidget(this);
    TargetsDock       = new TargetsFeedWidget(this);

    dockTop->toggleAction()->trigger();
    this->PlaceDock( dockTop, SessionsTableDock->dock() );
    dockBottom->toggleAction()->trigger();
    this->PlaceDock( dockBottom, LogsDock->dock() );

    TickThread = new QThread;
    TickWorker = new LastTickWorker( this );
    TickWorker->moveToThread( TickThread );

    connect( this, &AdaptixWidget::SyncedSignal, this,   &AdaptixWidget::OnSynced);
    connect( this, &AdaptixWidget::SyncedSignal, ScriptManager, &AxScriptManager::emitReadyClient);

    connect( logsButton,          &QPushButton::clicked, this, &AdaptixWidget::LoadLogsUI);
    connect( chatButton,          &QPushButton::clicked, this, &AdaptixWidget::LoadChatUI);
    connect( listenersButton,     &QPushButton::clicked, this, &AdaptixWidget::LoadListenersUI);
    connect( sessionsButton,      &QPushButton::clicked, this, &AdaptixWidget::SetSessionsTableUI);
    connect( graphButton,         &QPushButton::clicked, this, &AdaptixWidget::SetGraphUI);
    connect( tasksButton,         &QPushButton::clicked, this, &AdaptixWidget::SetTasksUI);
    connect( tunnelButton,        &QPushButton::clicked, this, &AdaptixWidget::LoadTunnelsUI);
    connect( downloadsButton,     &QPushButton::clicked, this, &AdaptixWidget::LoadDownloadsUI);
    connect( screensButton,       &QPushButton::clicked, this, &AdaptixWidget::LoadScreenshotsUI);
    connect( credsButton,         &QPushButton::clicked, this, &AdaptixWidget::LoadCredentialsUI);
    connect( targetsButton,       &QPushButton::clicked, this, &AdaptixWidget::LoadTargetsUI);
    connect( connStatusWidget,    &QPushButton::clicked, this, &AdaptixWidget::OnReconnect);
    connect( scriptManagerButton, &QPushButton::clicked, this, &AdaptixWidget::LoadScriptsUI);
    connect( codeEditorButton,    &QPushButton::clicked, this, &AdaptixWidget::LoadCodeEditorUI);
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

    extDocksPopover = extDocksButton->popover();
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
    extDocksButton->setPopoverContentWidget(extDocksContent);

    connect(extDocksListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString id = item->data(Qt::UserRole).toString();
        if (extDocksMap.contains(id) && extDocksMap[id].showCallback) {
            extDocksMap[id].showCallback();
        }
        extDocksButton->setPopoverOpened(false);
    });

    const int pos = (GlobalClient && GlobalClient->settings) ? GlobalClient->settings->data.ToolbarPosition : 0;
    const bool vertical = (pos == 2 || pos == 3);

    buildSegmentedGroups(vertical);

    buildToolbarLayout(pos);

    dockTop = new KDDockWidgets::QtWidgets::DockWidget(this->profile->GetProject()+"-Dock-Top", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockTop->setWidget(new QWidget());

    dockBottom = new KDDockWidgets::QtWidgets::DockWidget(this->profile->GetProject()+"-Dock-Bottom", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockBottom->setWidget(new QWidget());

    mainDockWidget = new KDDockWidgets::QtWidgets::MainWindow(this->profile->GetProject()+"-MainDock", KDDockWidgets::MainWindowOption_None);
    mainDockWidget->addDockWidget(dockTop, KDDockWidgets::Location_OnTop);
    mainDockWidget->addDockWidget(dockBottom, KDDockWidgets::Location_OnBottom);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->setHorizontalSpacing(0);

    placeToolbarInGrid(mainGridLayout, pos);
    mainGridLayout->addWidget(mainDockWidget, 1, 1, 1, 1);

    this->setLayout(mainGridLayout);

    applyThemeColorsToToolbar();
}


void AdaptixWidget::createButtons()
{
    auto mkIconBtn = [this](const QString& iconPath, const QString& tooltip) {
        auto* btn = new QPushButton(QIcon(iconPath), "", this);
        btn->setIconSize(QSize(24, 24));
        btn->setFixedSize(37, 28);
        btn->setToolTip(tooltip);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    sessionsButton = mkIconBtn(":/icons/format_list", "Session table");
    graphButton    = mkIconBtn(":/icons/graph",       "Session graph");
    tasksButton    = mkIconBtn(":/icons/job",         "Jobs & Tasks");

    listenersButton = mkIconBtn(":/icons/listeners", "Listeners & Sites");
    tunnelButton    = mkIconBtn(":/icons/vpn",       "Tunnels table");
    logsButton      = mkIconBtn(":/icons/logs",      "Notifications");

    downloadsButton = mkIconBtn(":/icons/downloads", "Downloads");
    targetsButton   = mkIconBtn(":/icons/devices",   "Targets table");
    credsButton     = mkIconBtn(":/icons/key",       "Credentials");
    screensButton   = mkIconBtn(":/icons/picture",   "Screens");
    keysButton      = mkIconBtn(":/icons/keyboard",  "Keystrokes");
    chatButton      = mkIconBtn(":/icons/chat",      "Chat");

    scriptManagerButton = mkIconBtn(":/icons/folder_code", "Script manager");
    codeEditorButton    = mkIconBtn(":/icons/code",        "Code editor");

    extDocksButton = new oclero::qlementine::PopoverButton("", QIcon(":/icons/extension"), this);
    extDocksButton->setIconSize(QSize(24, 24));
    extDocksButton->setFixedSize(37, 28);
    extDocksButton->setToolTip("Extensions Docks");
    extDocksButton->setShowArrowIndicator(false);
    extDocksButton->setFocusPolicy(Qt::NoFocus);

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
    // Infra: Listeners / Logs / Chat | Tunnels
    buildGroup(groupInfra, { listenersButton, logsButton, chatButton, tunnelButton }, {3});
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
        QPalette pal = toolbarWidget->palette();
        pal.setColor(QPalette::Window, t.backgroundColorMain2);
        toolbarWidget->setPalette(pal);
        toolbarWidget->setAutoFillBackground(true);
    }

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
        m_groupPainter = new GroupPainter(t.neutralColor, t.borderColor, 5.0, this);
        for (auto* f : { groupView, groupInfra, groupData, groupDev })
            if (f) f->installEventFilter(m_groupPainter);
    } else {
        static_cast<GroupPainter*>(m_groupPainter)->setColors(t.neutralColor, t.borderColor);
        for (auto* f : { groupView, groupInfra, groupData, groupDev })
            if (f) f->update();
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

    switch (position) {
    case 1:
        toolbarWidget->setFixedHeight(40);
        grid->addWidget(toolbarWidget, 2, 1, 1, 1);
        break;
    case 2:
        toolbarWidget->setFixedWidth(72);
        toolbarWidget->layout()->setContentsMargins(6, 8, 14, 8);
        grid->addWidget(toolbarWidget, 1, 0, 1, 1);
        break;
    case 3:
        toolbarWidget->setFixedWidth(72);
        toolbarWidget->layout()->setContentsMargins(14, 8, 6, 8);
        grid->addWidget(toolbarWidget, 1, 2, 1, 1);
        break;
    case 0:
    default:
        toolbarWidget->setFixedHeight(40);
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
    if (dock->isOpen()) {
        dock->setAsCurrentTab();
        return;
    }

    QString previousFocusedName;
    QString dockBeingAddedName = dock->uniqueName();
    KDDockWidgets::Core::Group* parentDockGroup = parentDock->group();

    if (KDDockWidgets::DockRegistry::self() && parentDockGroup) {
        auto* previousFocused = KDDockWidgets::DockRegistry::self()->focusedDockWidget();
        if (previousFocused)
            previousFocusedName = previousFocused->uniqueName();
    }

    parentDock->toggleAction()->trigger();
    parentDock->addDockWidgetAsTab(dock);
    parentDock->toggleAction()->trigger();

    if (!previousFocusedName.isEmpty() && previousFocusedName != dockBeingAddedName && parentDockGroup) {
        QTimer::singleShot(100, [previousFocusedName, dockBeingAddedName]() {
            if (KDDockWidgets::DockRegistry::self()) {
                auto* currentFocused = KDDockWidgets::DockRegistry::self()->focusedDockWidget();

                if (currentFocused && currentFocused->uniqueName() == dockBeingAddedName)
                    return;

                if (currentFocused && currentFocused->uniqueName() != previousFocusedName && currentFocused->uniqueName() != dockBeingAddedName)
                    return;

                auto* coreDw = KDDockWidgets::DockRegistry::self()->dockByName(previousFocusedName);
                if (coreDw && !coreDw->isCurrentTab())
                    coreDw->setAsCurrentTab();
            }
        });
    }
}

bool AdaptixWidget::AddExtension(ExtensionFile* ext)
{
    if (ScriptManager->ScriptList().contains(ext->FilePath)) {
        ext->Enabled = false;
        ext->Message = "Script already loaded";
        return false;
    }

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

bool AdaptixWidget::IsSynchronized() { return this->synchronized; }

void AdaptixWidget::Close()
{
    if (TickWorker) {
        disconnect(TickWorker, nullptr, this, nullptr);
        QMetaObject::invokeMethod(TickWorker, "stopWorker", Qt::BlockingQueuedConnection);
        TickWorker = nullptr;
    }
    if (TickThread) {
        TickThread->quit();
        TickThread->wait();
        TickThread = nullptr;
    }

    if (ChannelWsWorker) {
        disconnect(ChannelWsWorker, nullptr, this, nullptr);
        disconnect(ChannelWsWorker, nullptr, ScriptManager, nullptr);
        QMetaObject::invokeMethod(ChannelWsWorker, "stopWorker", Qt::BlockingQueuedConnection);
        ChannelWsWorker = nullptr;
    }
    if (ChannelThread) {
        ChannelThread->quit();
        ChannelThread->wait();
        ChannelThread = nullptr;
    }

    this->ClearAdaptix();

    LogsDock->deleteLater();
    ChatDock->deleteLater();
    ListenersDock->deleteLater();
    SessionsGraphDock->deleteLater();
    if (TasksDock) {
        TasksDock->deleteLater();
        TasksDock = nullptr;
    }
    TunnelsDock->deleteLater();
    DownloadsDock->deleteLater();
    ScreenshotsDock->deleteLater();
    if (CredentialsDock) {
        CredentialsDock->deleteLater();
        CredentialsDock = nullptr;
    }
    if (TargetsDock) {
        TargetsDock->deleteLater();
        TargetsDock = nullptr;
    }
    if (SessionsTableDock) {
        SessionsTableDock->deleteLater();
        SessionsTableDock = nullptr;
    }

    dockTop->deleteLater();
    dockBottom->deleteLater();
    mainDockWidget->deleteLater();

    delete dialogSyncPacket;
    dialogSyncPacket = nullptr;

    delete profile;
    profile = nullptr;
}

void AdaptixWidget::ClearAdaptix()
{
    synchronized = false;

    LogsDock->Clear();
    ChatDock->Clear();
    DownloadsDock->Clear();
    ScreenshotsDock->Clear();
    TasksDock->Clear();
    ListenersDock->Clear();
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

void AdaptixWidget::ChatUnreadIncrement()
{
    if (!chatBadge) {
        chatBadge = new oclero::qlementine::NotificationBadge(chatButton);
        chatBadge->setBackgroundColor(QColor(0xe0, 0x3e, 0x3e));
        chatBadge->setForegroundColor(Qt::white);
        chatBadge->setPadding(QMargins(3, 1, 3, 1));
        chatBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    int count = chatBadge->text().isEmpty() ? 0 : chatBadge->text().toInt();
    count++;
    chatBadge->setText(QString::number(count));
    QSize sz = chatBadge->sizeHint();
    chatBadge->setGeometry(chatButton->width() - sz.width() - 1, 1, sz.width(), sz.height());
    chatBadge->show();
    chatBadge->raise();
}

void AdaptixWidget::ChatUnreadClear()
{
    if (chatBadge) {
        chatBadge->setText("");
        chatBadge->hide();
    }
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
            Commander* commander = new Commander();
            commander->SetAgentType(agentName);

            RegAgentConfig config = {agentName, listener, os, commander, true};
            RegisterAgents.push_back(config);
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
            QString groupDesc = cgObj["groupDescription"].toString();
            QJsonArray cmdsArray = cgObj["commands"].toArray();

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
                    regAgent.commander->SetMainCommands(cg);
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
            QString groupDesc = groupObj["groupDescription"].toString();

            if (groupName.isEmpty())
                groupName = scriptName;

            CommandsGroup cg = parseCommandsGroup(groupName, groupObj["commands"].toArray());
            if (cg.commands.isEmpty())
                continue;

            cg.engine = engine;

            for (auto &regAgent : this->RegisterAgents) {
                if (regAgent.name != group.agentName || regAgent.os != group.os)
                    continue;
                bool listenerMatch = group.listenerType.isEmpty() || regAgent.listenerType.isEmpty() || regAgent.listenerType == group.listenerType;
                if (listenerMatch)
                    regAgent.commander->AddServerGroup(groupName, groupDesc, cg);
            }

            QReadLocker locker(&AgentsMapLock);
            for (auto agent : AgentsMap) {
                if (agent->data.Name != group.agentName || agent->data.Os != group.os)
                    continue;
                bool lMatch = group.listenerType.isEmpty() || agent->listenerType.isEmpty() || agent->listenerType == group.listenerType;
                if (lMatch)
                    agent->commander->AddServerGroup(groupName, groupDesc, cg);
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
    for (const auto &data : ScriptManager->ServerScriptList())
        result.append({data.name, data.description, data.enabled});
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
    for (auto regAgent : this->RegisterAgents) {
        if ( !agents.contains(regAgent.name) ) continue;
        if ( !listeners.empty() && !regAgent.listenerType.isEmpty() && !listeners.contains(regAgent.listenerType)) continue;
        if ( !os.empty() && !os.contains(regAgent.os) ) continue;
        commanders.append(regAgent.commander);
    }
    return commanders;
}

QList<Commander*> AdaptixWidget::GetCommandersAll() const
{
    QList<Commander*> commanders;
    for (auto regAgent : this->RegisterAgents)
        commanders.append(regAgent.commander);
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
        this->PlaceDock(dockBottom, agent->Console->dock() );
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

    if (tasksDock->isOpen()) {
        mainDockWidget->addDockWidget(outputDock, KDDockWidgets::Location_OnBottom, tasksDock);
        TasksOutputDockPlaced = true;
    } else {
        mainDockWidget->addDockWidget(outputDock, KDDockWidgets::Location_OnRight);
        TasksOutputDockPlaced = true;
    }
}

void AdaptixWidget::LoadFileBrowserUI(qint64 AgentId)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceDock(dockBottom, agent->GetFileBrowser()->dock() );
}

void AdaptixWidget::LoadProcessBrowserUI(qint64 AgentId)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceDock(dockBottom, agent->GetProcessBrowser()->dock() );
}

void AdaptixWidget::LoadTerminalUI(qint64 AgentId)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceDock(dockBottom, agent->GetTerminal()->dock() );
}

void AdaptixWidget::LoadShellUI(qint64 AgentId)
{
    QReadLocker locker(&AgentsMapLock);
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    locker.unlock();
    if (agent)
        this->PlaceDock(dockBottom, agent->GetShell()->dock() );
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

void AdaptixWidget::SetSessionsTableUI() const { this->PlaceDock(dockTop, SessionsTableDock->dock() ); }

void AdaptixWidget::SetGraphUI() const { this->PlaceDock(dockTop, SessionsGraphDock->dock() ); }

void AdaptixWidget::SetTasksUI() const
{
    if (!mainDockWidget || !TasksDock)
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
    } else if (!TasksDockPlaced) {
        mainDockWidget->addDockWidget(tasksDock, KDDockWidgets::Location_OnRight);
        TasksDockPlaced = true;
    } else {
        tasksDock->open();
    }

    tasksDock->setAsCurrentTab();
}

void AdaptixWidget::LoadScriptsUI() const { this->PlaceDock(dockBottom, ScriptsDock->dock()); }

void AdaptixWidget::LoadCodeEditorUI() const
{
    if (!mainDockWidget || !CodeEditorDock)
        return;

    auto* dock = CodeEditorDock->dock();

    if (!CodeEditorDockPlaced) {
        CodeEditorDockPlaced = true;
        auto* tasksDock = (TasksDock && TasksDockPlaced) ? TasksDock->dockTasks() : nullptr;
        if (tasksDock && tasksDock->isOpen())
            tasksDock->addDockWidgetAsTab(dock);
        else
            mainDockWidget->addDockWidget(dock, KDDockWidgets::Location_OnRight);
        dock->setAsCurrentTab();
        return;
    }

    if (!dock->isOpen())
        dock->open();
    dock->setAsCurrentTab();
}

void AdaptixWidget::OpenInDevTools(const QString& filePath)
{
    LoadCodeEditorUI();
    if (CodeEditorDock)
        CodeEditorDock->openScript(filePath);
}

void AdaptixWidget::LoadLogsUI() const { this->PlaceDock(dockBottom, LogsDock->dock() ); }

void AdaptixWidget::LoadChatUI() const {
    this->PlaceDock(dockBottom, ChatDock->dock());
    const_cast<AdaptixWidget*>(this)->ChatUnreadClear();
}

void AdaptixWidget::LoadListenersUI() const { this->PlaceDock(dockBottom, ListenersDock->dock() ); }

void AdaptixWidget::LoadTunnelsUI() const { this->PlaceDock(dockBottom, TunnelsDock->dock() ); }

void AdaptixWidget::LoadDownloadsUI() const { this->PlaceDock(dockBottom, DownloadsDock->dock() ); }

void AdaptixWidget::LoadScreenshotsUI() const { this->PlaceDock(dockBottom, ScreenshotsDock->dock() ); }

void AdaptixWidget::LoadCredentialsUI() const { this->PlaceDock(dockBottom, CredentialsDock->dock() ); }

void AdaptixWidget::LoadTargetsUI() const { this->PlaceDock(dockBottom, TargetsDock->dock() ); }

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
    if (!extDocksButton)
        return;

    bool empty = extDocksListWidget->count() == 0;
    extDocksListWidget->setVisible(!empty);
    extDocksEmptyLabel->setVisible(empty);

    extDocksButton->setPopoverOpened(true);
}
