#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/ConsoleTheme.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Utils/Convert.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QScrollBar>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QToolButton>
#include <QSpinBox>
#include <QFrame>
#include <algorithm>

REGISTER_DOCK_WIDGET(LogsWidget, "Logs", true)

LogsWidget::LogsWidget(const AdaptixWidget* w) : DockTab("Logs", w->GetProfile()->GetProject(), ":/icons/logs"), adaptixWidget(w)
{
    serverPageSize    = qBound(10, GlobalClient->settings->data.ConsolePageSize, 2000);
    autoLoadEarlier   = GlobalClient->settings->data.ConsoleAutoLoadEarlier;

    this->createUI();

    connect(logsConsoleTextEdit, &TextEditConsole::ctx_find, searchPanel, &SearchPanel::toggle);
    connect(logsConsoleTextEdit, &TextEditConsole::ctx_clear, logsConsoleTextEdit, &QPlainTextEdit::clear);

    auto* shortcutFind = new QShortcut(QKeySequence("Ctrl+F"), logsConsoleTextEdit);
    shortcutFind->setContext(Qt::WidgetShortcut);
    connect(shortcutFind, &QShortcut::activated, searchPanel, &SearchPanel::toggle);

    auto* shortcutClear = new QShortcut(QKeySequence("Ctrl+L"), logsConsoleTextEdit);
    shortcutClear->setContext(Qt::WidgetShortcut);
    connect(shortcutClear, &QShortcut::activated, logsConsoleTextEdit, &QPlainTextEdit::clear);

    auto* shortcutSelectAll = new QShortcut(QKeySequence("Ctrl+A"), logsConsoleTextEdit);
    shortcutSelectAll->setContext(Qt::WidgetShortcut);
    connect(shortcutSelectAll, &QShortcut::activated, logsConsoleTextEdit, &QPlainTextEdit::selectAll);

    connect(serverLogsTextEdit, &TextEditConsole::ctx_find, serverSearchPanel, &SearchPanel::toggle);
    connect(serverLogsTextEdit, &TextEditConsole::ctx_clear, this, &LogsWidget::clearServerLogsView);
    connect(loadEarlierButton,  &QToolButton::clicked, this, &LogsWidget::loadMoreServerPage);
    connect(loadAllButton,      &QToolButton::clicked, this, &LogsWidget::loadAllServerPages);
    connect(stopLoadButton,     &QToolButton::clicked, this, &LogsWidget::stopLoadAllServer);
    connect(jumpLatestButton,   &QToolButton::clicked, this, &LogsWidget::jumpToLatestServer);
    connect(autoLoadSwitch, &oclero::qlementine::Switch::toggled, this, [this](bool on) {
        autoLoadEarlier = on;
    });
    connect(pageSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        serverPageSize = qBound(10, v, 2000);
    });
    connect(serverLogsTextEdit->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (!autoLoadEarlier || !serverHasMore || serverLoadingPage || serverLoadAllPending || serverViewCleared)
            return;
        if (value <= 8)
            loadMoreServerPage();
    });

    connect(sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        rebuildCategoryCombo();
        applyFiltersFromUi(true);
    });
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        applyFiltersFromUi(true);
    });
    connect(searchEdit, &QLineEdit::returnPressed, this, [this]() {
        applyFiltersFromUi(true);
    });
    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.trimmed().isEmpty() && !filterContains.isEmpty())
            applyFiltersFromUi(true);
    });

    auto* serverShortcutFind = new QShortcut(QKeySequence("Ctrl+F"), serverLogsTextEdit);
    serverShortcutFind->setContext(Qt::WidgetShortcut);
    connect(serverShortcutFind, &QShortcut::activated, serverSearchPanel, &SearchPanel::toggle);

    auto* serverShortcutClear = new QShortcut(QKeySequence("Ctrl+L"), serverLogsTextEdit);
    serverShortcutClear->setContext(Qt::WidgetShortcut);
    connect(serverShortcutClear, &QShortcut::activated, this, &LogsWidget::clearServerLogsView);

    auto* serverShortcutSelectAll = new QShortcut(QKeySequence("Ctrl+A"), serverLogsTextEdit);
    serverShortcutSelectAll->setContext(Qt::WidgetShortcut);
    connect(serverShortcutSelectAll, &QShortcut::activated, serverLogsTextEdit, &QPlainTextEdit::selectAll);

    connect(&ConsoleThemeManager::instance(), &ConsoleThemeManager::themeChanged, this, &LogsWidget::applyTheme);
    connect(logsConsoleTextEdit, &TextEditConsole::ctx_bgToggled, this, [this](bool){ applyTheme(); });
    connect(serverLogsTextEdit,  &TextEditConsole::ctx_bgToggled, this, [this](bool){ applyTheme(); });

    applyTheme();
    updateHistoryBar();

    this->dockWidget->setWidget(this);

    connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, [this](bool current) {
        if (current && adaptixWidget)
            const_cast<AdaptixWidget*>(adaptixWidget)->LogsUnreadClear();
    });
    connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [this](bool open) {
        if (!open || !adaptixWidget || !dockWidget)
            return;
        auto* core = dockWidget->dockWidget();
        if (core && core->isCurrentTab())
            const_cast<AdaptixWidget*>(adaptixWidget)->LogsUnreadClear();
    });

    ReloadServerLogs();
}

LogsWidget::~LogsWidget() = default;

void LogsWidget::createUI()
{
    logsConsoleTextEdit = new TextEditConsole(this);
    logsConsoleTextEdit->setReadOnly(true);
    logsConsoleTextEdit->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");
    logsConsoleTextEdit->setAutoScrollEnabled(true);

    searchPanel = new SearchPanel(logsConsoleTextEdit, this);

    logsGridLayout = new QGridLayout(this);
    logsGridLayout->setContentsMargins(1, 1, 1, 1);
    logsGridLayout->setVerticalSpacing(1);
    logsGridLayout->addWidget( searchPanel,         0, 0, 1, 1);
    logsGridLayout->addWidget( logsConsoleTextEdit, 1, 0, 1, 1);

    logsWidget = new QWidget(this);
    logsWidget->setLayout(logsGridLayout);

    serverLogsHost = new QWidget(this);
    serverLogsHost->setObjectName(QStringLiteral("ServerLogsHost"));
    auto* hostLayout = new QVBoxLayout(serverLogsHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    serverLogsTextEdit = new TextEditConsole(serverLogsHost);
    serverLogsTextEdit->setReadOnly(true);
    serverLogsTextEdit->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");
    serverLogsTextEdit->setAutoScrollEnabled(true);
    hostLayout->addWidget(serverLogsTextEdit, 1);

    serverSearchPanel = new SearchPanel(serverLogsTextEdit, serverLogsHost);
    serverSearchPanel->raise();

    historyBar = new QFrame(serverLogsHost);
    historyBar->setObjectName(QStringLiteral("ConsoleHistoryBar"));

    historyToggleBtn = new QToolButton(historyBar);
    historyToggleBtn->setObjectName(QStringLiteral("HistBtnToggle"));
    historyToggleBtn->setIcon(QIcon(QStringLiteral(":/icons/settings")));
    historyToggleBtn->setToolTip(tr("History / pagination"));
    historyToggleBtn->setAutoRaise(true);
    historyToggleBtn->setCursor(Qt::PointingHandCursor);
    historyToggleBtn->setFocusPolicy(Qt::NoFocus);
    connect(historyToggleBtn, &QToolButton::clicked, this, [this]() {
        setHistoryBarExpanded(!historyExpanded);
    });

    historyContent = new QWidget(historyBar);
    historyContent->setObjectName(QStringLiteral("ConsoleHistoryContent"));

    autoLoadSwitch = new oclero::qlementine::Switch(historyContent);
    autoLoadSwitch->setFixedSize(34, 16);
    autoLoadSwitch->setToolTip(tr("Auto-load older logs when scrolling to the top"));
    autoLoadSwitch->setChecked(autoLoadEarlier);

    historyStatusLabel = new QLabel(QStringLiteral("—"), historyContent);
    historyStatusLabel->setObjectName(QStringLiteral("ConsoleHistoryStatus"));
    historyStatusLabel->setToolTip(tr("Loaded log items / total on server"));

    pageSizeLabel = new QLabel(tr("count"), historyContent);
    pageSizeLabel->setObjectName(QStringLiteral("ConsoleHistoryMuted"));
    pageSizeLabel->setToolTip(tr("Log page size"));

    pageSizeSpin = new QSpinBox(historyContent);
    pageSizeSpin->setRange(10, 2000);
    pageSizeSpin->setSingleStep(10);
    pageSizeSpin->setValue(serverPageSize);
    pageSizeSpin->setFixedWidth(64);
    pageSizeSpin->setToolTip(tr("Log page size"));
    pageSizeSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);

    auto makeHistBtn = [this](const QString& objectName, const QString& iconPath, const QString& text, const QString& tip) {
        auto* btn = new QToolButton(historyContent);
        btn->setObjectName(objectName);
        if (!iconPath.isEmpty())
            btn->setIcon(QIcon(iconPath));
        if (!text.isEmpty())
            btn->setText(text);
        btn->setToolTip(tip);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        if (!iconPath.isEmpty() && !text.isEmpty())
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        else if (iconPath.isEmpty())
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return btn;
    };

    auto makeSep = [this]() {
        auto* sep = new QFrame(historyContent);
        sep->setObjectName(QStringLiteral("ConsoleHistorySep"));
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setFixedWidth(1);
        return sep;
    };

    loadEarlierButton = makeHistBtn(QStringLiteral("HistBtnEarlier"), QStringLiteral(":/icons/arrow_drop_up"), tr("Earlier"), tr("Load older logs"));
    loadAllButton     = makeHistBtn(QStringLiteral("HistBtnAll"), QString(), tr("All"), tr("Load entire server log history"));
    stopLoadButton    = makeHistBtn(QStringLiteral("HistBtnStop"), QString(), tr("Stop"), tr("Stop loading logs"));
    jumpLatestButton  = makeHistBtn(QStringLiteral("HistBtnJump"), QStringLiteral(":/icons/double_arrow_down"), QString(), tr("Jump to latest (scroll down)"));
    stopLoadButton->setVisible(false);

    auto* contentLayout = new QHBoxLayout(historyContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(6);
    contentLayout->addWidget(autoLoadSwitch, 0);
    contentLayout->addWidget(historyStatusLabel, 0);
    contentLayout->addWidget(makeSep(), 0);
    contentLayout->addWidget(loadEarlierButton, 0);
    contentLayout->addWidget(makeSep(), 0);
    contentLayout->addWidget(loadAllButton, 0);
    contentLayout->addWidget(stopLoadButton, 0);
    contentLayout->addWidget(makeSep(), 0);
    contentLayout->addWidget(jumpLatestButton, 0);
    contentLayout->addWidget(makeSep(), 0);
    contentLayout->addWidget(pageSizeLabel, 0);
    contentLayout->addWidget(pageSizeSpin, 0);

    auto* histLayout = new QHBoxLayout(historyBar);
    histLayout->setContentsMargins(4, 2, 4, 2);
    histLayout->setSpacing(6);
    histLayout->addWidget(historyToggleBtn, 0);
    histLayout->addWidget(historyContent, 0);

    historyContent->setVisible(false);
    historyBar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    historyBar->raise();

    applyHistoryBarMetrics();
    applyHistoryBarStyle();
    serverLogsHost->installEventFilter(this);
    serverSearchPanel->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() { positionServerOverlays(); });

    auto* sourceLabel = new QLabel(QStringLiteral("Source:"), this);
    sourceCombo = new QComboBox(this);
    sourceCombo->setMinimumWidth(120);
    sourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    sourceCombo->addItem(QStringLiteral("All"), QString());
    sourceCombo->setToolTip(QStringLiteral("Log origin (events, server, listener, …)"));

    auto* categoryLabel = new QLabel(QStringLiteral("Category:"), this);
    categoryCombo = new QComboBox(this);
    categoryCombo->setMinimumWidth(140);
    categoryCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    categoryCombo->addItem(QStringLiteral("All"), QString());
    categoryCombo->setEnabled(false);
    categoryCombo->setToolTip(QStringLiteral("Category within selected source"));

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(QStringLiteral("Search source… e.g. beac  (Enter)"));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMinimumWidth(160);
    searchEdit->setToolTip(QStringLiteral("Case-insensitive substring anywhere in source (e.g. beac → listener:tcp_beacon)"));

    auto* sourceBar = new QHBoxLayout();
    sourceBar->setContentsMargins(2, 2, 2, 2);
    sourceBar->setSpacing(6);
    sourceBar->addWidget(sourceLabel);
    sourceBar->addWidget(sourceCombo);
    sourceBar->addWidget(categoryLabel);
    sourceBar->addWidget(categoryCombo);
    sourceBar->addWidget(searchEdit, 1);

    serverLogsLayout = new QGridLayout(this);
    serverLogsLayout->setContentsMargins(1, 1, 1, 1);
    serverLogsLayout->setVerticalSpacing(1);
    serverLogsLayout->addLayout( sourceBar,      0, 0, 1, 1);
    serverLogsLayout->addWidget( serverLogsHost, 1, 0, 1, 1);

    serverLogsWidget = new QWidget(this);
    serverLogsWidget->setLayout(serverLogsLayout);

    mainHSplitter = new QSplitter( Qt::Horizontal, this );
    mainHSplitter->setHandleWidth(3);
    mainHSplitter->addWidget(logsWidget);
    mainHSplitter->addWidget(serverLogsWidget);
    mainHSplitter->setStretchFactor(0, 1);
    mainHSplitter->setStretchFactor(1, 1);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->addWidget( mainHSplitter, 0, 0, 1, 1);

    this->setLayout( mainGridLayout );
}

void LogsWidget::applyHistoryBarMetrics()
{
    if (!historyBar)
        return;

    const AppTypography& ty = FontManager::instance().typography();
    const int barH   = ty.historyBarHeight;
    const int btnH   = ty.controlInnerH;
    const int sepH   = qMax(12, btnH - 6);
    const qreal s    = ty.baseSize / 10.0;
    const int iconSm = qMax(10, qRound(12 * s));
    const int iconMd = qMax(12, qRound(14 * s));
    const int toggleSz = qMax(24, btnH + 2);

    historyBar->setFixedHeight(barH);
    if (historyToggleBtn) {
        historyToggleBtn->setFixedSize(toggleSz, toggleSz);
        historyToggleBtn->setIconSize(QSize(iconMd, iconMd));
    }
    if (pageSizeSpin)
        pageSizeSpin->setFixedHeight(btnH);
    if (autoLoadSwitch)
        autoLoadSwitch->setFixedSize(qMax(30, qRound(34 * s)), qMax(14, qRound(16 * s)));

    auto applyBtn = [&](QToolButton* btn, int iconPx) {
        if (!btn)
            return;
        btn->setFixedHeight(btnH);
        if (!btn->icon().isNull())
            btn->setIconSize(QSize(iconPx, iconPx));
    };
    applyBtn(loadEarlierButton, iconSm);
    applyBtn(loadAllButton, iconMd);
    applyBtn(stopLoadButton, iconMd);
    applyBtn(jumpLatestButton, iconMd);
    if (jumpLatestButton)
        jumpLatestButton->setFixedWidth(qMax(24, btnH + 4));

    const auto seps = historyBar->findChildren<QFrame*>(QStringLiteral("ConsoleHistorySep"));
    for (QFrame* sep : seps)
        sep->setFixedHeight(sepH);

    if (auto* lay = qobject_cast<QHBoxLayout*>(historyBar->layout())) {
        if (historyExpanded)
            lay->setContentsMargins(6, 2, 8, 2);
        else
            lay->setContentsMargins(3, 2, 3, 2);
    }
}

void LogsWidget::setHistoryBarExpanded(bool on)
{
    historyExpanded = on;
    if (historyContent)
        historyContent->setVisible(on);
    if (historyToggleBtn) {
        historyToggleBtn->setToolTip(on ? tr("Collapse history controls") : tr("History / pagination"));
    }
    applyHistoryBarMetrics();
    historyBar->adjustSize();
    positionServerOverlays();
}

void LogsWidget::applyHistoryBarStyle()
{
    if (!historyBar)
        return;
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const int fontPx = FontManager::instance().typography().chromeFontPx;
    const QString monoFamily = FontManager::instance().typography().family;

    historyBar->setStyleSheet(QStringLiteral(
        "QFrame#ConsoleHistoryBar {"
        "  background-color: rgba(%1, %2, %3, 210);"
        "  border: 1px solid %4;"
        "  border-radius: 8px;"
        "}"
        "QLabel#ConsoleHistoryStatus {"
        "  color: %5;"
        "  font-size: %8px;"
        "  font-family: '%9';"
        "  padding: 0 4px 0 2px;"
        "  min-width: 52px;"
        "}"
        "QLabel#ConsoleHistoryMuted {"
        "  color: %6;"
        "  font-size: %8px;"
        "  padding-right: 2px;"
        "}"
        "QFrame#ConsoleHistorySep {"
        "  background-color: %4;"
        "  max-width: 1px;"
        "  margin: 2px 1px;"
        "  border: none;"
        "}"
        "QToolButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "  color: %5;"
        "  font-size: %8px;"
        "}"
        "QToolButton:hover { background-color: %7; }"
        "QToolButton:disabled { color: %6; }"
        "QToolButton#HistBtnStop { color: #E8A0A0; }"
        "QToolButton#HistBtnEarlier { padding-left: 3px; padding-right: 5px; }"
        "QToolButton#HistBtnJump { padding: 2px 4px; }"
        "QToolButton#HistBtnToggle {"
        "  padding: 2px;"
        "  border-radius: 6px;"
        "}"
        "QToolButton#HistBtnToggle:hover { background-color: %7; }"
    ).arg(t.backgroundColorMain3.red())
     .arg(t.backgroundColorMain3.green())
     .arg(t.backgroundColorMain3.blue())
     .arg(t.borderColor.name(),
          t.primaryColor.name(),
          t.secondaryColor.name(),
          t.backgroundColorMain4.name())
     .arg(fontPx)
     .arg(monoFamily));
}

void LogsWidget::positionHistoryBar()
{
    if (!serverLogsHost || !historyBar)
        return;
    historyBar->adjustSize();
    historyBar->raise();
    historyBar->show();
}

void LogsWidget::positionSearchPanel()
{
    if (!serverLogsHost || !serverSearchPanel || !serverSearchPanel->isVisible())
        return;
    serverSearchPanel->adjustSize();
    serverSearchPanel->raise();
}

void LogsWidget::positionServerOverlays()
{
    if (!serverLogsHost)
        return;

    constexpr int margin = 6;
    constexpr int gap = 4;

    int sbW = 0;
    if (serverLogsTextEdit && serverLogsTextEdit->verticalScrollBar() && serverLogsTextEdit->verticalScrollBar()->isVisible())
        sbW = serverLogsTextEdit->verticalScrollBar()->width();

    const int hostW = serverLogsHost->width();
    const bool searchVis = serverSearchPanel && serverSearchPanel->isVisible();
    const bool histVis = historyBar != nullptr;

    if (searchVis) {
        serverSearchPanel->setMaximumWidth(QWIDGETSIZE_MAX);
        serverSearchPanel->adjustSize();
    }
    if (histVis)
        historyBar->adjustSize();

    const int histW = histVis ? historyBar->width() : 0;
    const int histH = histVis ? historyBar->height() : 0;
    const int searchW = searchVis ? serverSearchPanel->width() : 0;

    const int needSideBySide = margin + searchW + gap + histW + margin + sbW;
    const bool stack = searchVis && histVis && needSideBySide > hostW;

    if (histVis) {
        const int histX = qMax(margin, hostW - histW - margin - sbW);
        historyBar->move(histX, margin);
        historyBar->show();
        historyBar->raise();
    }

    if (searchVis) {
        int searchY = margin;
        if (stack) {
            searchY = margin + histH + gap;
            const int maxSearchW = qMax(120, hostW - 2 * margin);
            if (serverSearchPanel->width() > maxSearchW) {
                serverSearchPanel->setMaximumWidth(maxSearchW);
                serverSearchPanel->adjustSize();
            }
        } else {
            const int maxAlone = qMax(120, hostW - 2 * margin);
            if (serverSearchPanel->width() > maxAlone) {
                serverSearchPanel->setMaximumWidth(maxAlone);
                serverSearchPanel->adjustSize();
            }
        }
        serverSearchPanel->move(margin, searchY);
        serverSearchPanel->raise();
    }
}

bool LogsWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == serverLogsHost && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
        positionServerOverlays();
    }
    if (watched == serverSearchPanel && (event->type() == QEvent::Show || event->type() == QEvent::Hide || event->type() == QEvent::Resize)) {
        QTimer::singleShot(0, this, [this]() { positionServerOverlays(); });
    }
    return DockTab::eventFilter(watched, event);
}

void LogsWidget::updateHistoryBar()
{
    const bool busy = serverLoadingPage || serverLoadAllPending;

    QString statusText = QStringLiteral("—");
    if (historyStatusLabel) {
        if (serverLoadAllPending)
            statusText = tr("Loading %1 / %2…").arg(serverLoadedCount).arg(serverTotalKnown > 0 ? serverTotalKnown : serverLoadedCount);
        else if (serverTotalKnown > 0)
            statusText = QStringLiteral("%1 / %2").arg(serverLoadedCount).arg(serverTotalKnown);
        else if (serverLoadedCount > 0)
            statusText = QString::number(serverLoadedCount);
        historyStatusLabel->setText(statusText);
    }

    if (historyToggleBtn && !historyExpanded) {
        historyToggleBtn->setToolTip(tr("History: %1 — click to expand").arg(statusText));
    }

    if (loadEarlierButton) {
        const bool canEarlier = serverHasMore && !busy && (oldestLoadedId > 0 || serverViewCleared || !serverLogsReady);
        loadEarlierButton->setEnabled(canEarlier);
    }
    if (loadAllButton) {
        loadAllButton->setEnabled(serverHasMore && !busy);
        loadAllButton->setVisible(!serverLoadAllPending);
    }
    if (stopLoadButton)
        stopLoadButton->setVisible(serverLoadAllPending);
    if (jumpLatestButton)
        jumpLatestButton->setEnabled(true);
    if (pageSizeSpin)
        pageSizeSpin->setEnabled(!busy);

    positionServerOverlays();
}

int LogsWidget::effectiveLoadLimit() const
{
    if (serverLoadAllPending)
        return qBound(serverPageSize, 500, 2000);
    return serverPageSize;
}

void LogsWidget::finishBulkLoad()
{
    if (serverLogsTextEdit) {
        serverLogsTextEdit->setBulkInsertMode(false);
        serverLogsTextEdit->setUpdatesEnabled(true);
    }
}

void LogsWidget::SetUpdatesEnabled(const bool enabled)
{
    logsConsoleTextEdit->setUpdatesEnabled(enabled);
    if (serverLogsTextEdit) {
        serverLogsTextEdit->setUpdatesEnabled(enabled);
    }
}

ConsoleThemeData LogsWidget::getActiveTheme() const
{
    if (GlobalClient->settings->data.ConsoleUseAppTheme)
        return ConsoleThemeManager::buildFromQlementine( GlobalClient->settings->data.MainTheme, GlobalClient->settings->data.ConsoleBgImagePath, GlobalClient->settings->data.ConsoleBgDimming);
    return ConsoleThemeManager::instance().theme();
}

void LogsWidget::AddLogs(const int type, const qint64 time, const QString &message)
{
    const auto theme = getActiveTheme();

    QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString logTime = QString("[%1] -> ").arg(sTime);
    logsConsoleTextEdit->appendFormatted(logTime, [&](QTextCharFormat& fmt){ fmt = theme.logDebug.toFormat(); });

    QString logMsg = message + "\n";

    if( type == EVENT_CLIENT_CONNECT )         logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.operatorConnect.toFormat(); });
    else if( type == EVENT_CLIENT_DISCONNECT ) logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.operatorDisconnect.toFormat(); });
    else if( type == EVENT_LISTENER_START )    logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.listenerStart.toFormat(); });
    else if( type == EVENT_LISTENER_STOP )     logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.listenerStop.toFormat(); });
    else if( type == EVENT_AGENT_NEW )         logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.agentNew.toFormat(); });
    else if( type == EVENT_TUNNEL_START )      logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.tunnel.toFormat(); });
    else if( type == EVENT_TUNNEL_STOP )       logsConsoleTextEdit->appendFormatted(logMsg, [&](QTextCharFormat& fmt){ fmt = theme.tunnel.toFormat(); });
    else                                       logsConsoleTextEdit->appendPlain(logMsg);
}

void LogsWidget::applyTheme()
{
    const auto theme = getActiveTheme();
    const auto& bg = theme.background;
    bool showBg = GlobalClient->settings->data.ConsoleShowBackground;

    QString imagePath;
    int dimming = bg.dimming;
    if (showBg) {
        if (bg.type == ConsoleBackground::Image && !bg.imagePath.isEmpty()) {
            imagePath = bg.imagePath;
        } else {
            QString settingsPath = GlobalClient->settings->data.ConsoleBgImagePath;
            if (settingsPath.isEmpty())
                settingsPath = ":/Back";
            if (QFile::exists(settingsPath)) {
                imagePath = settingsPath;
                dimming = GlobalClient->settings->data.ConsoleBgDimming;
            }
        }
    }
    logsConsoleTextEdit->setConsoleBackground(bg.color, imagePath, dimming);

    logsConsoleTextEdit->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background-color: transparent; color: %1; border: 1px solid #2A2A2A; border-radius: 4px; }")
            .arg(theme.textColor.name()));

    if (serverLogsTextEdit) {
        serverLogsTextEdit->setConsoleBackground(bg.color, imagePath, dimming);
        serverLogsTextEdit->setStyleSheet(
            QStringLiteral("QPlainTextEdit { background-color: transparent; color: %1; border: 1px solid #2A2A2A; border-radius: 4px; }")
                .arg(theme.textColor.name()));
    }

    applyHistoryBarStyle();
    applyHistoryBarMetrics();
    positionServerOverlays();
}

void LogsWidget::Clear()
{
    if (logsConsoleTextEdit)
        logsConsoleTextEdit->clear();
    clearServerLogsView();
}

void LogsWidget::clearServerLogsView()
{
    stopLoadAllServer();
    ++serverLogsEpoch;
    serverLoadingPage = false;

    if (serverLogsTextEdit) {
        serverLogsTextEdit->clear();
        serverLogsTextEdit->resetHistoryCount();
    }
    if (serverSearchPanel)
        serverSearchPanel->clearSelections();

    pendingServerLogs.clear();
    seenLogIds.clear();
    oldestLoadedId    = 0;
    serverLoadedCount = 0;
    serverHasMore     = true;
    serverLogsReady   = true;
    serverViewCleared = true;
    updateHistoryBar();
}

QString LogsWidget::displaySource(const QString& source, const QString& category)
{
    if (category.isEmpty())
        return source;
    if (source.isEmpty())
        return category;
    return source + QLatin1String("::") + category;
}

bool LogsWidget::matchesSourceFilter(const QString& source, const QString& category) const
{
    if (!filterOrigin.isEmpty() && source != filterOrigin)
        return false;
    if (!filterCategory.isEmpty() && category != filterCategory)
        return false;
    if (!filterContains.isEmpty()) {
        const QString key = displaySource(source, category);
        if (!source.contains(filterContains, Qt::CaseInsensitive) && !category.contains(filterContains, Qt::CaseInsensitive) && !key.contains(filterContains, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

void LogsWidget::applyFiltersFromUi(bool reload)
{
    const QString nextOrigin = sourceCombo
        ? sourceCombo->currentData().toString()
        : QString();
    const QString nextCategory = (categoryCombo && categoryCombo->isEnabled())
        ? categoryCombo->currentData().toString()
        : QString();
    const QString nextContains = searchEdit
        ? searchEdit->text().trimmed()
        : QString();

    if (nextOrigin == filterOrigin && nextCategory == filterCategory && nextContains == filterContains)
        return;

    filterOrigin   = nextOrigin;
    filterCategory = nextCategory;
    filterContains = nextContains;

    if (reload) {
        ResetServerLogs();
        loadInitialServerPage();
    }
}

void LogsWidget::rebuildSourceCombo()
{
    if (!sourceCombo)
        return;

    const QString keep = filterOrigin;
    sourceCombo->blockSignals(true);
    sourceCombo->clear();
    sourceCombo->addItem(QStringLiteral("All"), QString());

    QStringList origins = knownOrigins.values();
    origins.sort(Qt::CaseInsensitive);
    for (const QString& o : origins)
        sourceCombo->addItem(o, o);

    int found = 0;
    for (int i = 0; i < sourceCombo->count(); ++i) {
        if (sourceCombo->itemData(i).toString() == keep) {
            found = i;
            break;
        }
    }
    sourceCombo->setCurrentIndex(found);
    sourceCombo->blockSignals(false);

    rebuildCategoryCombo();
}

void LogsWidget::rebuildCategoryCombo()
{
    if (!categoryCombo)
        return;

    const QString origin = sourceCombo ? sourceCombo->currentData().toString() : QString();
    const QString keepCat = (origin == filterOrigin) ? filterCategory : QString();

    categoryCombo->blockSignals(true);
    categoryCombo->clear();
    categoryCombo->addItem(QStringLiteral("All"), QString());

    if (origin.isEmpty()) {
        categoryCombo->setEnabled(false);
        categoryCombo->setCurrentIndex(0);
        categoryCombo->blockSignals(false);
        return;
    }

    categoryCombo->setEnabled(true);
    QStringList cats = originCategories.value(origin).values();
    cats.erase(std::remove_if(cats.begin(), cats.end(), [](const QString& c) { return c.trimmed().isEmpty(); }), cats.end());
    cats.sort(Qt::CaseInsensitive);
    for (const QString& c : cats)
        categoryCombo->addItem(c, c);

    int found = 0;
    for (int i = 0; i < categoryCombo->count(); ++i) {
        if (categoryCombo->itemData(i).toString() == keepCat) {
            found = i;
            break;
        }
    }
    categoryCombo->setCurrentIndex(found);
    categoryCombo->blockSignals(false);
}

void LogsWidget::noteSource(const QString& source, const QString& category)
{
    if (source.isEmpty())
        return;

    bool changed = false;
    if (!knownOrigins.contains(source)) {
        knownOrigins.insert(source);
        changed = true;
    }
    if (!category.isEmpty()) {
        auto& set = originCategories[source];
        if (!set.contains(category)) {
            set.insert(category);
            changed = true;
        }
    }

    if (changed)
        rebuildSourceCombo();
}

void LogsWidget::applyCatalogFromResponse(const QJsonObject& response)
{
    bool any = false;

    if (response.contains(QStringLiteral("sources")) && response.value(QStringLiteral("sources")).isArray()) {
        for (const QJsonValue& v : response.value(QStringLiteral("sources")).toArray()) {
            if (!v.isString() || v.toString().isEmpty())
                continue;
            if (!knownOrigins.contains(v.toString())) {
                knownOrigins.insert(v.toString());
                any = true;
            }
        }
    }

    if (response.contains(QStringLiteral("categories")) && response.value(QStringLiteral("categories")).isObject()) {
        const QJsonObject cats = response.value(QStringLiteral("categories")).toObject();
        for (auto it = cats.begin(); it != cats.end(); ++it) {
            const QString origin = it.key();
            if (origin.isEmpty())
                continue;
            if (!knownOrigins.contains(origin)) {
                knownOrigins.insert(origin);
                any = true;
            }
            if (!it.value().isArray())
                continue;
            auto& set = originCategories[origin];
            for (const QJsonValue& cv : it.value().toArray()) {
                if (!cv.isString() || cv.toString().isEmpty())
                    continue;
                if (!set.contains(cv.toString())) {
                    set.insert(cv.toString());
                    any = true;
                }
            }
        }
    }

    if (any)
        rebuildSourceCombo();
}

void LogsWidget::appendServerLogEntry(qint64 id, qint64 time, int status, int level, const QString& source, const QString& category, const QString& message)
{
    Q_UNUSED(id);

    if (!serverLogsTextEdit)
        return;
    if (!matchesSourceFilter(source, category))
        return;

    const auto theme = getActiveTheme();

    QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString prefix = QString("[%1] ").arg(sTime);
    serverLogsTextEdit->appendFormatted(prefix, [&](QTextCharFormat& fmt){ fmt = theme.logDebug.toFormat(); });

    QString tag;
    QColor  tagColor;
    switch (status) {
        case LOG_STATUS_DEBUG:   tag = "[#] "; tagColor = theme.statusDebug;   break;
        case LOG_STATUS_INFO:    tag = "[*] "; tagColor = theme.statusInfo;    break;
        case LOG_STATUS_SUCCESS: tag = "[+] "; tagColor = theme.statusSuccess; break;
        case LOG_STATUS_WARN:    tag = "[!] "; tagColor = theme.statusWarn;    break;
        case LOG_STATUS_ERROR:   tag = "[-] "; tagColor = theme.statusError;   break;
        default:                 tag = "[?] "; tagColor = theme.textColor;     break;
    }
    serverLogsTextEdit->appendFormatted(tag, [&](QTextCharFormat& fmt){ fmt.setForeground(tagColor); fmt.setFontWeight(QFont::Bold); });

    constexpr int kLogSourcePadMin = 24;
    QString key = displaySource(source, category);
    if (key.isEmpty())
        key = QStringLiteral("server");
    const QString srcTag = QStringLiteral("[%1]").arg(key);
    QString pad;
    if (key.size() < kLogSourcePadMin)
        pad = QString(kLogSourcePadMin - key.size(), QLatin1Char('.'));
    serverLogsTextEdit->appendFormatted(srcTag, [&](QTextCharFormat& fmt){ fmt.setForeground(QColor("#888888")); });
    if (!pad.isEmpty())
        serverLogsTextEdit->appendFormatted(pad, [&](QTextCharFormat& fmt){ fmt.setForeground(QColor("#555555")); });
    serverLogsTextEdit->appendFormatted(QStringLiteral(" "), [&](QTextCharFormat&){});

    QString indent;
    for (int i = 0; i < level; ++i)
        indent += "  ";

    QString msg = indent + message + "\n";
    serverLogsTextEdit->appendFormatted(msg, [&](QTextCharFormat& fmt){ fmt.setForeground(theme.textColor); });
}

void LogsWidget::AddServerLogBatch(const QJsonArray& items)
{
    if (!serverLogsReady) {
        for (const auto& v : items) {
            if (!v.isObject())
                continue;
            pendingServerLogs.append(v.toObject());
        }
        return;
    }

    int rendered = 0;
    for (const auto& v : items) {
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        const qint64 id = parseI64(obj, QStringLiteral("id"));
        if (id > 0 && seenLogIds.contains(id))
            continue;

        const QString src = obj.value(QStringLiteral("source")).toString();
        const QString cat = obj.value(QStringLiteral("category")).toString();
        noteSource(src, cat);
        if (!matchesSourceFilter(src, cat))
            continue;

        appendServerLogEntry( id, parseI64(obj, QStringLiteral("time")), obj.value(QStringLiteral("status")).toInt(), obj.value(QStringLiteral("level")).toInt(), src, cat, obj.value(QStringLiteral("message")).toString() );
        if (id > 0) {
            seenLogIds.insert(id);
            if (oldestLoadedId <= 0 || id < oldestLoadedId)
                oldestLoadedId = id;
        }
        ++rendered;
    }
    if (rendered > 0) {
        serverLoadedCount = seenLogIds.isEmpty()
            ? (serverLoadedCount + rendered)
            : seenLogIds.size();
        if (serverTotalKnown < serverLoadedCount)
            serverTotalKnown = serverLoadedCount;
        updateHistoryBar();
    }
}

void LogsWidget::ResetServerLogs()
{
    if (serverLogsTextEdit)
        serverLogsTextEdit->clear();
    if (serverSearchPanel)
        serverSearchPanel->clearSelections();
    serverLogsReady       = false;
    pendingServerLogs.clear();
    seenLogIds.clear();
    oldestLoadedId        = 0;
    serverLoadedCount     = 0;
    serverTotalKnown      = 0;
    serverHasMore         = true;
    serverLoadingPage     = false;
    serverLoadAllPending  = false;
    serverViewCleared     = false;
    finishBulkLoad();
    updateHistoryBar();
    ++serverLogsEpoch;
}

void LogsWidget::ReloadServerLogs()
{
    loadInitialServerPage();
}

void LogsWidget::loadInitialServerPage()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile())
        return;
    if (serverLoadingPage)
        return;

    if (serverLogsTextEdit)
        serverLogsTextEdit->clear();
    if (serverSearchPanel)
        serverSearchPanel->clearSelections();

    serverLogsReady   = false;
    serverViewCleared = false;
    serverLoadingPage = true;
    serverLoadedCount = 0;
    serverTotalKnown  = 0;
    serverHasMore     = true;
    oldestLoadedId    = 0;
    seenLogIds.clear();
    updateHistoryBar();

    const int epoch = ++serverLogsEpoch;
    const int limit = effectiveLoadLimit();
    const QString origin   = filterOrigin;
    const QString category = filterCategory;
    const QString contains = filterContains;

    AuthProfile* profile = adaptixWidget->GetProfile();
    QPointer<LogsWidget> self = this;
    HttpReqLogsGetPageAsync(0, limit, 0, origin, category, contains, *profile, [self, epoch, limit](bool success, const QString& message, const QJsonObject& response) {
        Q_UNUSED(message);
        if (!self)
            return;
        if (epoch != self->serverLogsEpoch)
            return;
        if (!self->serverLogsTextEdit)
            return;

        self->serverLoadingPage = false;
        if (!success) {
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
            return;
        }

        const QJsonArray items = response.value(QStringLiteral("items")).toArray();
        const int total = response.value(QStringLiteral("total")).toInt();
        self->applyCatalogFromResponse(response);

        if (items.isEmpty()) {
            self->serverLogsReady = true;
            self->serverViewCleared = false;
            if (!self->pendingServerLogs.isEmpty()) {
                QJsonArray pending;
                for (const auto& o : self->pendingServerLogs)
                    pending.append(o);
                self->pendingServerLogs.clear();
                self->AddServerLogBatch(pending);
            }
            self->serverLoadedCount = self->seenLogIds.size();
            self->serverTotalKnown  = qMax(total, self->serverLoadedCount);
            if (response.contains(QStringLiteral("has_more")))
                self->serverHasMore = response.value(QStringLiteral("has_more")).toBool();
            else
                self->serverHasMore = false;
            if (self->oldestLoadedId <= 0)
                self->serverHasMore = false;
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
            return;
        }

        qint64 minId = 0;
        int    rendered = 0;
        self->serverLogsTextEdit->setSyncMode(true);
        for (int i = items.size() - 1; i >= 0; --i) {
            const QJsonValue v = items.at(i);
            if (!v.isObject())
                continue;
            const QJsonObject obj = v.toObject();
            const qint64 id = parseI64(obj, QStringLiteral("id"));
            if (id > 0 && (minId == 0 || id < minId))
                minId = id;
            if (id > 0 && self->seenLogIds.contains(id))
                continue;
            const qint64 t = parseI64(obj, QStringLiteral("time"));
            const int st = obj.value(QStringLiteral("status")).toInt();
            const int lv = obj.value(QStringLiteral("level")).toInt();
            const QString src = obj.value(QStringLiteral("source")).toString();
            const QString cat = obj.value(QStringLiteral("category")).toString();
            const QString msg = obj.value(QStringLiteral("message")).toString();

            self->noteSource(src, cat);
            self->appendServerLogEntry(id, t, st, lv, src, cat, msg);
            if (id > 0)
                self->seenLogIds.insert(id);
            ++rendered;
        }
        self->serverLogsTextEdit->setSyncMode(false);

        const qint64 serverOldest = parseI64(response, QStringLiteral("oldest_id"));
        if (serverOldest > 0)
            minId = serverOldest;
        if (minId > 0)
            self->oldestLoadedId = minId;

        self->serverLogsReady = true;
        self->serverViewCleared = false;
        if (!self->pendingServerLogs.isEmpty()) {
            QJsonArray pending;
            for (const auto& o : self->pendingServerLogs)
                pending.append(o);
            self->pendingServerLogs.clear();
            self->AddServerLogBatch(pending);
        }

        self->serverLoadedCount = self->seenLogIds.isEmpty()
            ? rendered
            : self->seenLogIds.size();
        self->serverTotalKnown = total > 0
            ? qMax(total, self->serverLoadedCount)
            : self->serverLoadedCount;

        if (response.contains(QStringLiteral("has_more")))
            self->serverHasMore = response.value(QStringLiteral("has_more")).toBool();
        else
            self->serverHasMore = (items.size() >= limit) || (self->serverTotalKnown > 0 && self->serverLoadedCount < self->serverTotalKnown);
        if (self->oldestLoadedId <= 0)
            self->serverHasMore = false;

        self->updateHistoryBar();

        if (self->serverLogsTextEdit && !self->serverLoadAllPending) {
            auto* sb = self->serverLogsTextEdit->verticalScrollBar();
            if (sb)
                sb->setValue(sb->maximum());
        }

        if (self->serverLoadAllPending && self->serverHasMore) {
            QTimer::singleShot(0, self, [self, epoch]() {
                if (self && self->serverLogsEpoch == epoch && self->serverLoadAllPending)
                    self->loadMoreServerPage();
            });
        } else if (self->serverLoadAllPending) {
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
        } else if (self->autoLoadEarlier && !self->serverViewCleared && self->serverHasMore && self->serverLogsTextEdit) {
            QTimer::singleShot(50, self, [self, epoch]() {
                if (!self || self->serverLogsEpoch != epoch || !self->serverLogsTextEdit || self->serverLoadingPage || !self->serverHasMore || self->serverViewCleared)
                    return;
                auto* bar = self->serverLogsTextEdit->verticalScrollBar();
                if (bar && (bar->maximum() <= 0 || bar->value() <= 8))
                    self->loadMoreServerPage();
            });
        }
    });
}

void LogsWidget::loadMoreServerPage()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile() || serverLoadingPage)
        return;

    serverViewCleared = false;

    if (oldestLoadedId <= 0) {
        if (!serverHasMore && !serverLoadAllPending) {
            updateHistoryBar();
            return;
        }
        loadInitialServerPage();
        return;
    }

    if (!serverHasMore) {
        if (serverLoadAllPending) {
            serverLoadAllPending = false;
            finishBulkLoad();
        }
        updateHistoryBar();
        return;
    }

    serverLoadingPage = true;
    updateHistoryBar();

    const qint64 beforeId = oldestLoadedId;
    const int    epoch    = serverLogsEpoch;
    const int    limit    = effectiveLoadLimit();
    const QString origin   = filterOrigin;
    const QString category = filterCategory;
    const QString contains = filterContains;

    AuthProfile* profile = adaptixWidget->GetProfile();
    QPointer<LogsWidget> self = this;
    HttpReqLogsGetPageAsync(0, limit, beforeId, origin, category, contains, *profile,  [self, epoch, limit, beforeId](bool success, const QString& message, const QJsonObject& response) {
        Q_UNUSED(message);
        if (!self)
            return;
        if (epoch != self->serverLogsEpoch)
            return;
        if (!self->serverLogsTextEdit) {
            self->serverLoadingPage = false;
            return;
        }

        self->serverLoadingPage = false;
        if (!success) {
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
            return;
        }

        self->applyCatalogFromResponse(response);

        const QJsonArray items = response.value(QStringLiteral("items")).toArray();
        const int total = response.value(QStringLiteral("total")).toInt();
        if (items.isEmpty()) {
            self->serverLoadedCount = self->seenLogIds.isEmpty()
                ? self->serverLoadedCount
                : self->seenLogIds.size();
            if (total > 0)
                self->serverTotalKnown = qMax(total, self->serverLoadedCount);
            self->serverHasMore = false;
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
            return;
        }

        auto* sb = self->serverLogsTextEdit->verticalScrollBar();
        const int oldValue = sb ? sb->value() : 0;
        const int oldMax   = sb ? sb->maximum() : 0;
        const qint64 prevOldest = self->oldestLoadedId;

        qint64 minId = 0;
        int    rendered = 0;
        self->serverLogsTextEdit->beginPrepend();
        for (int i = items.size() - 1; i >= 0; --i) {
            const QJsonValue v = items.at(i);
            if (!v.isObject())
                continue;
            const QJsonObject obj = v.toObject();
            const qint64 id = parseI64(obj, QStringLiteral("id"));
            if (id > 0 && (minId == 0 || id < minId))
                minId = id;
            if (id > 0 && self->seenLogIds.contains(id))
                continue;
            const qint64 t = parseI64(obj, QStringLiteral("time"));
            const int st = obj.value(QStringLiteral("status")).toInt();
            const int lv = obj.value(QStringLiteral("level")).toInt();
            const QString src = obj.value(QStringLiteral("source")).toString();
            const QString cat = obj.value(QStringLiteral("category")).toString();
            const QString msg = obj.value(QStringLiteral("message")).toString();

            self->noteSource(src, cat);
            self->appendServerLogEntry(id, t, st, lv, src, cat, msg);
            if (id > 0)
                self->seenLogIds.insert(id);
            ++rendered;
        }
        self->serverLogsTextEdit->endPrepend();

        if (sb) {
            const int newMax = sb->maximum();
            sb->setValue(oldValue + (newMax - oldMax));
        }

        const qint64 serverOldest = parseI64(response, QStringLiteral("oldest_id"));
        if (serverOldest > 0)
            minId = serverOldest;

        if (minId > 0 && (prevOldest <= 0 || minId < prevOldest))
            self->oldestLoadedId = minId;

        self->serverLoadedCount = self->seenLogIds.isEmpty()
            ? (self->serverLoadedCount + rendered)
            : self->seenLogIds.size();
        if (total > 0)
            self->serverTotalKnown = qMax(total, self->serverLoadedCount);
        else if (self->serverTotalKnown < self->serverLoadedCount)
            self->serverTotalKnown = self->serverLoadedCount;

        if (response.contains(QStringLiteral("has_more")))
            self->serverHasMore = response.value(QStringLiteral("has_more")).toBool();
        else
            self->serverHasMore = (items.size() >= limit) || (self->serverTotalKnown > 0 && self->serverLoadedCount < self->serverTotalKnown);

        const bool cursorMoved = (self->oldestLoadedId > 0 && self->oldestLoadedId < prevOldest) || (prevOldest <= 0 && self->oldestLoadedId > 0);
        if (!cursorMoved && rendered == 0)
            self->serverHasMore = false;
        if (beforeId > 0 && minId >= beforeId && rendered == 0)
            self->serverHasMore = false;

        if (self->serverSearchPanel)
            self->serverSearchPanel->clearSelections();
        self->updateHistoryBar();

        if (self->serverLoadAllPending && self->serverHasMore) {
            QTimer::singleShot(0, self, [self, epoch]() {
                if (self && self->serverLogsEpoch == epoch && self->serverLoadAllPending)
                    self->loadMoreServerPage();
            });
        } else if (self->serverLoadAllPending) {
            self->serverLoadAllPending = false;
            self->finishBulkLoad();
            self->updateHistoryBar();
        } else if (self->autoLoadEarlier && !self->serverViewCleared && self->serverHasMore && self->serverLogsTextEdit) {
            QTimer::singleShot(50, self, [self, epoch]() {
                if (!self || self->serverLogsEpoch != epoch || !self->serverLogsTextEdit || self->serverLoadingPage || !self->serverHasMore || self->serverViewCleared)
                    return;
                auto* bar = self->serverLogsTextEdit->verticalScrollBar();
                if (bar && (bar->maximum() <= 0 || bar->value() <= 8))
                    self->loadMoreServerPage();
            });
        }
    });
}

void LogsWidget::loadAllServerPages()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile() || serverLoadingPage || serverLoadAllPending)
        return;
    if (!serverHasMore)
        return;

    serverViewCleared = false;
    serverLoadAllPending = true;
    if (serverLogsTextEdit) {
        serverLogsTextEdit->setUpdatesEnabled(false);
        serverLogsTextEdit->setBulkInsertMode(true);
    }
    updateHistoryBar();

    if (!serverLogsReady || oldestLoadedId <= 0)
        loadInitialServerPage();
    else
        loadMoreServerPage();
}

void LogsWidget::stopLoadAllServer()
{
    serverLoadAllPending = false;
    finishBulkLoad();
    updateHistoryBar();
}

void LogsWidget::reloadLatestServerPage()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile())
        return;
    stopLoadAllServer();
    serverViewCleared = false;
    loadInitialServerPage();
}

void LogsWidget::jumpToLatestServer()
{
    if (!serverLogsTextEdit)
        return;

    if (serverViewCleared && seenLogIds.isEmpty()) {
        reloadLatestServerPage();
        return;
    }

    auto* sb = serverLogsTextEdit->verticalScrollBar();
    if (sb)
        sb->setValue(sb->maximum());
}
