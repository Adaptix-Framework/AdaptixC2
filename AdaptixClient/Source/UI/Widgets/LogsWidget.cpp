#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/ConsoleTheme.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Utils/Convert.h>
#include <MainAdaptix.h>
#include <QScrollBar>
#include <QPointer>

REGISTER_DOCK_WIDGET(LogsWidget, "Logs", true)

LogsWidget::LogsWidget(const AdaptixWidget* w) : DockTab("Logs", w->GetProfile()->GetProject(), ":/icons/logs"), adaptixWidget(w)
{
    this->createUI();

    connect(logsConsoleTextEdit, &TextEditConsole::ctx_find, searchPanel, &SearchPanel::toggle);

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
    connect(loadEarlierButton,  &ClickableLabel::clicked,   this,              &LogsWidget::loadMoreServerPage);

    auto* serverShortcutFind = new QShortcut(QKeySequence("Ctrl+F"), serverLogsTextEdit);
    serverShortcutFind->setContext(Qt::WidgetShortcut);
    connect(serverShortcutFind, &QShortcut::activated, serverSearchPanel, &SearchPanel::toggle);

    auto* serverShortcutClear = new QShortcut(QKeySequence("Ctrl+L"), serverLogsTextEdit);
    serverShortcutClear->setContext(Qt::WidgetShortcut);
    connect(serverShortcutClear, &QShortcut::activated, serverLogsTextEdit, &QPlainTextEdit::clear);

    auto* serverShortcutSelectAll = new QShortcut(QKeySequence("Ctrl+A"), serverLogsTextEdit);
    serverShortcutSelectAll->setContext(Qt::WidgetShortcut);
    connect(serverShortcutSelectAll, &QShortcut::activated, serverLogsTextEdit, &QPlainTextEdit::selectAll);

    connect(&ConsoleThemeManager::instance(), &ConsoleThemeManager::themeChanged, this, &LogsWidget::applyTheme);
    connect(logsConsoleTextEdit, &TextEditConsole::ctx_bgToggled, this, [this](bool){ applyTheme(); });
    connect(serverLogsTextEdit,  &TextEditConsole::ctx_bgToggled, this, [this](bool){ applyTheme(); });

    applyTheme();

    this->dockWidget->setWidget(this);

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

    serverLogsTextEdit = new TextEditConsole(this);
    serverLogsTextEdit->setReadOnly(true);
    serverLogsTextEdit->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");
    serverLogsTextEdit->setAutoScrollEnabled(true);

    serverSearchPanel = new SearchPanel(serverLogsTextEdit, this);

    loadEarlierButton = new ClickableLabel("▲ Load earlier");
    loadEarlierButton->setCursor(Qt::PointingHandCursor);
    loadEarlierButton->setAlignment(Qt::AlignCenter);
    loadEarlierButton->setStyleSheet("padding: 4px; color: #7FA3C0; background-color: transparent; border: none;");
    loadEarlierButton->setVisible(false);

    auto* serverTopBar = new QHBoxLayout();
    serverTopBar->setContentsMargins(0, 0, 0, 0);
    serverTopBar->setSpacing(4);
    serverTopBar->addWidget(serverSearchPanel, 1);
    serverTopBar->addWidget(loadEarlierButton, 1);

    serverLogsLayout = new QGridLayout(this);
    serverLogsLayout->setContentsMargins(1, 1, 1, 1);
    serverLogsLayout->setVerticalSpacing(1);
    serverLogsLayout->addLayout( serverTopBar,        0, 0, 1, 1);
    serverLogsLayout->addWidget( serverLogsTextEdit,  1, 0, 1, 1);

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
}

void LogsWidget::Clear() const
{
    logsConsoleTextEdit->clear();
    if (serverLogsTextEdit) {
        serverLogsTextEdit->clear();
    }
}

void LogsWidget::appendServerLogEntry(qint64 id, qint64 time, int status, int level, const QString& source, const QString& message)
{
    Q_UNUSED(id);

    if (!serverLogsTextEdit)
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

    if (!source.isEmpty()) {
        QString src = QString("[%1] ").arg(source);
        serverLogsTextEdit->appendFormatted(src, [&](QTextCharFormat& fmt){ fmt.setForeground(QColor("#888888")); });
    }

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
        const qint64 id = static_cast<qint64>(obj.value("id").toDouble());
        if (id > 0 && seenLogIds.contains(id))
            continue;

        appendServerLogEntry(
            id,
            static_cast<qint64>(obj.value("time").toDouble()),
            obj.value("status").toInt(),
            obj.value("level").toInt(),
            obj.value("source").toString(),
            obj.value("message").toString()
        );
        if (id > 0) seenLogIds.insert(id);
        ++rendered;
    }
    if (rendered > 0) {
        serverLoadedCount += rendered;
        serverTotalKnown  += rendered;
        updateLoadEarlierVisibility();
    }
}

void LogsWidget::ResetServerLogs()
{
    if (serverLogsTextEdit)
        serverLogsTextEdit->clear();
    if (serverSearchPanel)
        serverSearchPanel->clearSelections();
    serverLogsReady = false;
    pendingServerLogs.clear();
    seenLogIds.clear();
    oldestLoadedId    = 0;
    serverLoadedCount = 0;
    serverTotalKnown  = 0;
    serverLoadingPage = false;
    updateLoadEarlierVisibility();
    ++serverLogsEpoch;
}

void LogsWidget::ReloadServerLogs()
{
    loadInitialServerPage();
}

void LogsWidget::updateLoadEarlierVisibility()
{
    if (!loadEarlierButton)
        return;
    bool hasMore = serverLoadedCount < serverTotalKnown;
    loadEarlierButton->setVisible(hasMore || serverLoadingPage);
    loadEarlierButton->setText(serverLoadingPage
        ? QStringLiteral("Loading...")
        : QStringLiteral("▲ Load earlier (%1 / %2)").arg(serverLoadedCount).arg(serverTotalKnown));
}

void LogsWidget::loadInitialServerPage()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile())
        return;
    if (serverLoadingPage)
        return;

    serverLogsReady   = false;
    serverLoadingPage = true;
    serverLoadedCount = 0;
    serverTotalKnown  = 0;
    oldestLoadedId    = 0;
    seenLogIds.clear();
    updateLoadEarlierVisibility();

    const qint64 beforeId = oldestLoadedId;
    const int epoch = ++serverLogsEpoch;

    AuthProfile* profile = adaptixWidget->GetProfile();
    QPointer<LogsWidget> self = this;
    HttpReqLogsGetPageAsync(0, serverPageSize, beforeId, *profile, [self, epoch](bool success, const QString& message, const QJsonObject& response) {
        Q_UNUSED(message);
        if (!self)
            return;
        if (epoch != self->serverLogsEpoch)
            return;
        if (!self->serverLogsTextEdit)
            return;

        self->serverLoadingPage = false;
        if (!success) {
            self->updateLoadEarlierVisibility();
            return;
        }

        const QJsonArray items = response.value("items").toArray();
        const int total = response.value("total").toInt();
        if (items.isEmpty()) {
            self->serverTotalKnown = total;
            self->updateLoadEarlierVisibility();
            return;
        }

        auto* sb = self->serverLogsTextEdit->verticalScrollBar();
        const int oldValue = sb->value();
        const int oldMax   = sb->maximum();

        qint64 minId = 0;
        int    rendered = 0;
        self->serverLogsTextEdit->beginPrepend();
        for (int i = items.size() - 1; i >= 0; --i) {
            const QJsonValue v = items.at(i);
            if (!v.isObject())
                continue;
            const QJsonObject obj = v.toObject();
            qint64  id = static_cast<qint64>(obj.value("id").toDouble());
            if (id > 0 && self->seenLogIds.contains(id))
                continue;
            qint64  t  = static_cast<qint64>(obj.value("time").toDouble());
            int     st = obj.value("status").toInt();
            int     lv = obj.value("level").toInt();
            QString src = obj.value("source").toString();
            QString msg = obj.value("message").toString();

            self->appendServerLogEntry(id, t, st, lv, src, msg);
            if (id > 0) self->seenLogIds.insert(id);
            if (id > 0 && (minId == 0 || id < minId)) minId = id;
            ++rendered;
        }
        self->serverLogsTextEdit->endPrepend();

        const int newMax = sb->maximum();
        sb->setValue(oldValue + (newMax - oldMax));

        if (minId > 0) self->oldestLoadedId = minId;
        self->serverLoadedCount += rendered;
        self->serverTotalKnown   = total;
        self->updateLoadEarlierVisibility();
    });
}

void LogsWidget::loadMoreServerPage()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile() || serverLoadingPage || serverLoadedCount >= serverTotalKnown || oldestLoadedId <= 0)
        return;

    serverLoadingPage = true;
    updateLoadEarlierVisibility();

    const qint64 beforeId = oldestLoadedId;
    const int    epoch    = serverLogsEpoch;

    AuthProfile* profile = adaptixWidget->GetProfile();
    QPointer<LogsWidget> self = this;
    HttpReqLogsGetPageAsync(0, serverPageSize, beforeId, *profile, [self, epoch](bool success, const QString& message, const QJsonObject& response) {
        Q_UNUSED(message);
        if (!self)
            return;
        if (epoch != self->serverLogsEpoch)
            return;
        if (!self->serverLogsTextEdit)
            return;

        self->serverLoadingPage = false;
        if (!success) {
            self->updateLoadEarlierVisibility();
            return;
        }

        const QJsonArray items = response.value("items").toArray();
        const int total = response.value("total").toInt();
        if (items.isEmpty()) {
            self->serverTotalKnown = total;
            self->updateLoadEarlierVisibility();
            return;
        }

        auto* sb = self->serverLogsTextEdit->verticalScrollBar();
        const int oldValue = sb->value();
        const int oldMax   = sb->maximum();

        qint64 minId = 0;
        int    rendered = 0;
        self->serverLogsTextEdit->beginPrepend();
        // server returns newest-first; reverse so oldest from this batch renders first
        for (int i = items.size() - 1; i >= 0; --i) {
            const QJsonValue v = items.at(i);
            if (!v.isObject())
                continue;
            const QJsonObject obj = v.toObject();
            qint64  id = static_cast<qint64>(obj.value("id").toDouble());
            if (id > 0 && self->seenLogIds.contains(id))
                continue;
            qint64  t  = static_cast<qint64>(obj.value("time").toDouble());
            int     st = obj.value("status").toInt();
            int     lv = obj.value("level").toInt();
            QString src = obj.value("source").toString();
            QString msg = obj.value("message").toString();

            self->appendServerLogEntry(id, t, st, lv, src, msg);
            if (id > 0)
                self->seenLogIds.insert(id);
            if (id > 0 && (minId == 0 || id < minId))
                minId = id;
            ++rendered;
        }
        self->serverLogsTextEdit->endPrepend();

        const int newMax = sb->maximum();
        sb->setValue(oldValue + (newMax - oldMax));

        if (minId > 0) self->oldestLoadedId = minId;
        self->serverLoadedCount += rendered;
        self->serverTotalKnown   = total;
        self->updateLoadEarlierVisibility();
    });
}
