#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogUploader.h>
#include <UI/Dialogs/DialogConsoleSearch.h>
#include <UI/Dialogs/DialogConsoleHelp.h>
#include <UI/Widgets/FilesFeedWidget.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <Client/ConsoleTheme.h>
#include <Utils/FontManager.h>
#include <Utils/Convert.h>
#include <MainAdaptix.h>
#include <QDateTime>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QRandomGenerator>
#include <QPointer>
#include <QScrollBar>
#include <QToolButton>
#include <QTimer>
#include <QEvent>
#include <QTextCursor>
#include <QTextLayout>
#include <QPainter>
#include <cmath>

class PromptStrip : public QWidget
{
    QString m_text;
    QColor  m_color;

public:
    explicit PromptStrip(QWidget* parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);
    }

    void setText(const QString& text)
    {
        if (m_text == text)
            return;
        m_text = text;
        update();
    }

    void setColor(const QColor& color)
    {
        if (m_color == color)
            return;
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_text.isEmpty())
            return;
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(m_color.isValid() ? m_color : palette().color(QPalette::Text));
        const int gap = 8;
        const int x = qMax(0, width() - gap - fontMetrics().horizontalAdvance(m_text));
        painter.drawText(x, fontMetrics().ascent(), m_text);
    }
};

class ConsoleInputEdit : public QPlainTextEdit
{
    PromptStrip* m_promptStrip = nullptr;
    QString      m_prompt;
    int          m_promptPx = 0;

public:
    explicit ConsoleInputEdit(QWidget* parent = nullptr) : QPlainTextEdit(parent)
    {
        m_promptStrip = new PromptStrip(this);
        connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { positionPrompt(); });
    }

    void setPrompt(const QString& text, int widthPx)
    {
        const int px = qMax(0, widthPx);
        const bool changed = m_prompt != text || m_promptPx != px;
        m_prompt = text;
        m_promptPx = px;
        if (changed) {
            setViewportMargins(px, 0, 0, 0);
        }
        if (m_promptStrip) {
            m_promptStrip->setText(text);
        }
        positionPrompt();
    }

    void setPromptColor(const QColor& color)
    {
        if (m_promptStrip)
            m_promptStrip->setColor(color);
    }

    int promptWidth() const { return m_promptPx; }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QPlainTextEdit::resizeEvent(event);
        positionPrompt();
    }

    void showEvent(QShowEvent* event) override
    {
        QPlainTextEdit::showEvent(event);
        positionPrompt();
    }

private:
    void positionPrompt()
    {
        if (!m_promptStrip) {
            return;
        }
        if (m_promptPx <= 0 || m_prompt.isEmpty()) {
            m_promptStrip->hide();
            return;
        }

        m_promptStrip->setFont(font());
        const QRect viewportRect = viewport()->geometry();
        QTextCursor start(document());
        start.movePosition(QTextCursor::Start);
        const QRect caret = cursorRect(start);
        const int lineTop = (caret.isValid() && caret.height() >= 2) ? caret.top() : 0;
        const int lineH = (caret.isValid() && caret.height() >= 2) ? caret.height() : fontMetrics().lineSpacing();
        m_promptStrip->setGeometry(0, viewportRect.top() + lineTop, m_promptPx, qMax(lineH, fontMetrics().lineSpacing()));
        m_promptStrip->show();
        m_promptStrip->raise();
    }
};

REGISTER_DOCK_WIDGET(ConsoleWidget, "Agent Console", false)

ConsoleWidget::ConsoleWidget( AdaptixWidget* w, Agent* a, Commander* c) : DockTab(QString("Console [%1]").arg( a->data.Id ), w->GetProfile()->GetProject(), QString(), w)
{
    adaptixWidget = w;
    agent         = a;
    commander     = c;

    pageSize        = qBound(10, GlobalClient->settings->data.ConsolePageSize, 2000);
    autoLoadEarlier = GlobalClient->settings->data.ConsoleAutoLoadEarlier;

    this->createUI();
    this->upgradeCompleter();

    connect(CommandCompleter, QOverload<const QString &>::of(&QCompleter::activated), this, &ConsoleWidget::onCompletionSelected, Qt::DirectConnection);
    connect(adaptixWidget,    &AdaptixWidget::agentTickUpdated, this, [this](qint64 agentId) {
        if (agent && agent->data.Id == agentId)
            this->UpdateStatusLabel();
    });
    connect(OutputTextEdit,   &TextEditConsole::ctx_find,                             searchPanel, &SearchPanel::toggle);
    connect(OutputTextEdit,   &TextEditConsole::ctx_history,                          this, &ConsoleWidget::handleShowHistory);
    connect(OutputTextEdit,   &TextEditConsole::ctx_help,                             this, &ConsoleWidget::handleShowHelp);
    connect(OutputTextEdit,   &TextEditConsole::ctx_clear,                            this, &ConsoleWidget::Clear);
    connect(commander,        &Commander::commandsUpdated,                            this, &ConsoleWidget::upgradeCompleter);

    auto* shortcutFind = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutFind->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutFind, &QShortcut::activated, searchPanel, &SearchPanel::toggle);

    auto* shortcutClear = new QShortcut(QKeySequence("Ctrl+L"), this);
    shortcutClear->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutClear, &QShortcut::activated, this, &ConsoleWidget::Clear);

    auto* shortcutSelectAll = new QShortcut(QKeySequence("Ctrl+A"), OutputTextEdit);
    shortcutSelectAll->setContext(Qt::WidgetShortcut);
    connect(shortcutSelectAll, &QShortcut::activated, OutputTextEdit, &QPlainTextEdit::selectAll);

    auto* shortcutHistory = new QShortcut(QKeySequence("Ctrl+H"), this);
    shortcutHistory->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutHistory, &QShortcut::activated, this, &ConsoleWidget::handleShowHistory);

    auto* shortcutHelp = new QShortcut(QKeySequence("Ctrl+Shift+H"), this);
    shortcutHelp->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutHelp, &QShortcut::activated, this, &ConsoleWidget::handleShowHelp);

    auto* shortcutServerSearch = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    shortcutServerSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutServerSearch, &QShortcut::activated, this, &ConsoleWidget::openHistorySearch);

    kphInputLineEdit = new KPH_ConsoleInput(InputLineEdit, OutputTextEdit, this);
    kphInputLineEdit->setCommandModel(completerModel);
    kphInputLineEdit->setCompleter(CommandCompleter);
    kphInputLineEdit->setCompleterPopup(CommandCompleter->popup());
    InputLineEdit->installEventFilter(kphInputLineEdit);
    connect(kphInputLineEdit, &KPH_ConsoleInput::showHistory, this, &ConsoleWidget::handleShowHistory);
    connect(kphInputLineEdit, &KPH_ConsoleInput::showHelp, this, &ConsoleWidget::handleShowHelp);
    connect(kphInputLineEdit, &KPH_ConsoleInput::submitInput, this, &ConsoleWidget::processInput, Qt::QueuedConnection);

    connect(&ConsoleThemeManager::instance(), &ConsoleThemeManager::themeChanged, this, [this]() {
        applyTheme();
        applyHistoryBarStyle();
    });
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        const QFont mono = FontManager::instance().appMonoFont();
        if (InputLineEdit)
            InputLineEdit->setFont(mono);
        refitInputHeight();
        if (OutputTextEdit)
            OutputTextEdit->setFont(mono);
        applyHistoryBarMetrics();
        applyHistoryBarStyle();
        positionConsoleOverlays();
    });
    connect(OutputTextEdit, &TextEditConsole::ctx_bgToggled, this, [this](bool){ applyTheme(); });
    connect(loadEarlierButton, &QToolButton::clicked, this, &ConsoleWidget::loadMorePage);
    connect(loadAllButton, &QToolButton::clicked, this, &ConsoleWidget::loadAllPages);
    connect(stopLoadButton, &QToolButton::clicked, this, &ConsoleWidget::stopLoadAll);
    connect(jumpLatestButton, &QToolButton::clicked, this, &ConsoleWidget::jumpToLatest);
    connect(autoLoadSwitch, &oclero::qlementine::Switch::toggled, this, [this](bool on) {
        autoLoadEarlier = on;
    });
    connect(pageSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        pageSize = qBound(10, v, 2000);
    });
    connect(searchPanel, &SearchPanel::historySearchRequested, this, &ConsoleWidget::openHistorySearch);
    connect(OutputTextEdit->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (!autoLoadEarlier || !hasMore || loadingPage || loadAllPending || !initialLoaded || viewCleared)
            return;
        if (value <= 8)
            loadMorePage();
    });
    connect(OutputTextEdit->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) {
        positionConsoleOverlays();
    });
    applyTheme();

    this->dockWidget->setWidget(this);
}

ConsoleWidget::~ConsoleWidget() {}

void ConsoleWidget::SetCommander(Commander* c)
{
    if (commander == c)
        return;

    if (commander)
        disconnect(commander, &Commander::commandsUpdated, this, &ConsoleWidget::upgradeCompleter);

    commander = c;

    if (commander)
        connect(commander, &Commander::commandsUpdated, this, &ConsoleWidget::upgradeCompleter);

    upgradeCompleter();
}

void ConsoleWidget::SetUpdatesEnabled(const bool enabled)
{
    OutputTextEdit->setUpdatesEnabled(enabled);
    if (!OutputTextEdit->isPrependMode())
        OutputTextEdit->setSyncMode(!enabled);
}

void ConsoleWidget::createUI()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const QString textColor   = t.secondaryColor.name();
    const QString borderColor = t.borderColor.name();
    const QString inputBg     = t.backgroundColorMain4.name();

    inputFrame = new QFrame(this);
    inputFrame->setObjectName(QStringLiteral("ConsoleInputFrame"));
    inputFrame->setAttribute(Qt::WA_StyledBackground, true);
    inputFrame->setStyleSheet(QStringLiteral("QFrame#ConsoleInputFrame { background-color: %1; border: 1px solid %2; border-radius: 4px; }").arg(inputBg, borderColor));

    InputLineEdit = new ConsoleInputEdit(inputFrame);
    InputLineEdit->setFrameShape(QFrame::NoFrame);
    InputLineEdit->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; color: %1; border: none; padding: 0px; }").arg(textColor));
    InputLineEdit->setFont( FontManager::instance().getFont("Hack") );
    InputLineEdit->document()->setDocumentMargin(0);
    InputLineEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    InputLineEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    InputLineEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    InputLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    InputLineEdit->setFixedHeight(inputLineHeight());
    InputLineEdit->setToolTip(tr("The command stays one line and wraps when it does not fit"));
    InputLineEdit->setPromptColor(QColor(textColor));
    InputLineEdit->installEventFilter(this);
    connect(InputLineEdit, &QPlainTextEdit::textChanged, this, &ConsoleWidget::refitInputHeight);
    connect(InputLineEdit, &QPlainTextEdit::textChanged, this, &ConsoleWidget::syncCompleter);

    auto* inputLayout = new QHBoxLayout(inputFrame);
    inputLayout->setContentsMargins(8, 2, 6, 2);
    inputLayout->setSpacing(0);
    inputLayout->addWidget(InputLineEdit, 1);
    inputFrame->installEventFilter(this);
    placeInputPrompt();

    InfoLabel = new QLabel(this);
    InfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    InfoLabel->setStyleSheet(QStringLiteral("padding: 4px; color: %1; background-color: transparent;").arg(textColor));
    this->UpdateInfoLabel();

    StatusLabel = new QLabel(this);
    StatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    StatusLabel->setStyleSheet(QStringLiteral("padding: 4px; color: %1; background-color: transparent;").arg(textColor));
    this->UpdateStatusLabel();

    consoleHost = new QWidget(this);
    consoleHost->setObjectName(QStringLiteral("ConsoleHost"));

    OutputTextEdit = new TextEditConsole(consoleHost, GlobalClient->settings->data.ConsoleBufferSize, GlobalClient->settings->data.ConsoleNoWrap, GlobalClient->settings->data.ConsoleAutoScroll);
    OutputTextEdit->setReadOnly(true);
    OutputTextEdit->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");
    OutputTextEdit->setFont( FontManager::instance().getFont("Hack") );

    auto* hostLayout = new QVBoxLayout(consoleHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(OutputTextEdit, 1);

    searchPanel = new SearchPanel(OutputTextEdit, consoleHost);
    searchPanel->setHistorySearchEnabled(true);
    searchPanel->raise();

    historyBar = new QFrame(consoleHost);
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
    autoLoadSwitch->setToolTip(tr("Auto-load older history when scrolling to the top"));
    autoLoadSwitch->setChecked(autoLoadEarlier);

    historyStatusLabel = new QLabel(QStringLiteral("—"), historyContent);
    historyStatusLabel->setObjectName(QStringLiteral("ConsoleHistoryStatus"));
    historyStatusLabel->setToolTip(tr("Loaded history items / total on server"));

    pageSizeLabel = new QLabel(tr("count"), historyContent);
    pageSizeLabel->setObjectName(QStringLiteral("ConsoleHistoryMuted"));
    pageSizeLabel->setToolTip(tr("History page size"));

    pageSizeSpin = new QSpinBox(historyContent);
    pageSizeSpin->setRange(10, 2000);
    pageSizeSpin->setSingleStep(10);
    pageSizeSpin->setValue(pageSize);
    pageSizeSpin->setFixedWidth(64);
    pageSizeSpin->setToolTip(tr("History page size"));
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

    loadEarlierButton = makeHistBtn(QStringLiteral("HistBtnEarlier"), QStringLiteral(":/icons/arrow_drop_up"), tr("Earlier"), tr("Load older history"));
    loadAllButton     = makeHistBtn(QStringLiteral("HistBtnAll"), QString(), tr("All"), tr("Load entire console history"));
    stopLoadButton    = makeHistBtn(QStringLiteral("HistBtnStop"), QString(), tr("Stop"), tr("Stop loading history"));
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
    consoleHost->installEventFilter(this);
    searchPanel->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() { positionConsoleOverlays(); });

    auto* infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(4);
    infoLayout->addWidget(InfoLabel, 1);
    infoLayout->addWidget(StatusLabel, 0);

    MainGridLayout = new QGridLayout(this );
    MainGridLayout->setVerticalSpacing(4 );
    MainGridLayout->setContentsMargins(0, 1, 0, 4 );
    MainGridLayout->addWidget( consoleHost,   0, 0, 1, 1 );
    MainGridLayout->addLayout( infoLayout,    1, 0, 1, 1 );
    MainGridLayout->addWidget( inputFrame, 2, 0, 1, 1 );
    MainGridLayout->setRowStretch(0, 1);
    MainGridLayout->setColumnStretch(0, 1);

    completerModel = new QStringListModel();
    CommandCompleter = new QCompleter(completerModel, this);
    CommandCompleter->popup()->setObjectName("Completer");
    CommandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    CommandCompleter->setCompletionMode(QCompleter::PopupCompletion);
    CommandCompleter->setWidget(InputLineEdit);
}



void ConsoleWidget::upgradeCompleter() const
{
    if (commander)
        completerModel->setStringList(commander->GetCommands());
}

void ConsoleWidget::InputFocus() const { InputLineEdit->setFocus(); }

int ConsoleWidget::inputLineHeight() const
{
    if (!InputLineEdit)
        return 28;
    return InputLineEdit->fontMetrics().lineSpacing() + 4;
}

void ConsoleWidget::refitInputHeight()
{
    if (!InputLineEdit)
        return;

    const QString raw = InputLineEdit->toPlainText();
    QString flat = raw;
    flat.replace(QLatin1Char('\r'), QLatin1Char(' '));
    flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (flat != raw) {
        const int pos = qMin(InputLineEdit->textCursor().position(), flat.size());
        InputLineEdit->blockSignals(true);
        InputLineEdit->setPlainText(flat);
        QTextCursor cursor = InputLineEdit->textCursor();
        cursor.setPosition(pos);
        InputLineEdit->setTextCursor(cursor);
        InputLineEdit->blockSignals(false);
    }

    placeInputPrompt();

    const int one = inputLineHeight();
    const QFontMetrics fm = InputLineEdit->fontMetrics();
    const int lineH = qMax(fm.lineSpacing(), fm.height());
    int textWidth = InputLineEdit->viewport()->width();
    if (textWidth < 40)
        textWidth = qMax(40, InputLineEdit->width() - InputLineEdit->promptWidth() - 8);

    int lines = 0;
    const QString text = InputLineEdit->toPlainText();
    if (!text.isEmpty() && textWidth > 0) {
        QTextLayout layout(text, InputLineEdit->font());
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(textWidth);
            ++lines;
        }
        layout.endLayout();
    }
    lines = qMax(1, lines);

    const int want = qMax(one, lines * lineH + 4);
    const int maxH = qMax(one, height() - 160);
    const int next = qBound(one, want, maxH);
    const bool wrapped = next > one + 2;
    if (InputLineEdit->verticalScrollBarPolicy() != (wrapped && want > next ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff))
        InputLineEdit->setVerticalScrollBarPolicy(wrapped && want > next ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (InputLineEdit->height() != next)
        InputLineEdit->setFixedHeight(next);
}

void ConsoleWidget::placeInputPrompt()
{
    if (!InputLineEdit)
        return;
    const QFontMetrics fm(InputLineEdit->font());
    const QString suffix = QStringLiteral(" >");
    const QString fullName = agent ? agent->data.Name : QString();
    const int gap = 8;
    const int fieldW = qMax(160, InputLineEdit->width());
    const int maxPrompt = qBound(72, fieldW / 3, 160);
    const int nameBudget = qMax(24, maxPrompt - fm.horizontalAdvance(suffix) - gap);
    const QString shown = fm.elidedText(fullName, Qt::ElideRight, nameBudget) + suffix;
    InputLineEdit->setPrompt(shown, fm.horizontalAdvance(shown) + gap);
    InputLineEdit->setToolTip(fullName.isEmpty() ? QString() : fullName + suffix);
}

void ConsoleWidget::syncCompleter()
{
    if (!CommandCompleter || !InputLineEdit)
        return;
    const QString text = InputLineEdit->toPlainText();
    const int pos = InputLineEdit->textCursor().position();
    const QString left = text.left(pos);
    if (left.contains(QLatin1Char(' ')) || left.contains(QLatin1Char('\n'))) {
        CommandCompleter->popup()->hide();
        return;
    }
    CommandCompleter->setCompletionPrefix(left);
    if (left.isEmpty() || CommandCompleter->completionCount() == 0) {
        CommandCompleter->popup()->hide();
        return;
    }
    QRect cr = InputLineEdit->cursorRect();
    cr.moveTopLeft(InputLineEdit->viewport()->mapTo(InputLineEdit, cr.topLeft()));
    cr.setWidth(qMax(180, CommandCompleter->popup()->sizeHintForColumn(0) + 24));
    cr.translate(0, 4);
    CommandCompleter->complete(cr);
}

void ConsoleWidget::LoadInitialPage() { loadInitialPage(); }

void ConsoleWidget::AddToHistory(const QString &command) { kphInputLineEdit->AddToHistory(command); }

void ConsoleWidget::SetInput(const QString &command)
{
    if (!InputLineEdit)
        return;
    InputLineEdit->setPlainText(command);
    refitInputHeight();
}

void ConsoleWidget::resetViewState()
{
    if (OutputTextEdit) {
        OutputTextEdit->clear();
        OutputTextEdit->resetHistoryCount();
    }
    m_consoleTaskPrompted.clear();
    m_consoleTaskClosed.clear();
    m_consoleTaskMsgKeys.clear();
    loadedItemCount = 0;
    oldestLoadedId  = 0;
    if (searchPanel)
        searchPanel->clearSelections();
}

void ConsoleWidget::Clear()
{
    stopLoadAll();
    ++loadGen;
    loadingPage = false;

    resetViewState();
    hasMore       = true;
    initialLoaded = true;
    viewCleared   = true;
    historyWindow = false;
    updateHistoryBar();
}

void ConsoleWidget::ApplyConsolePrefs()
{
    pageSize = qBound(10, GlobalClient->settings->data.ConsolePageSize, 2000);
    autoLoadEarlier = GlobalClient->settings->data.ConsoleAutoLoadEarlier;
    if (pageSizeSpin) {
        pageSizeSpin->blockSignals(true);
        pageSizeSpin->setValue(pageSize);
        pageSizeSpin->blockSignals(false);
    }
    if (autoLoadSwitch) {
        autoLoadSwitch->blockSignals(true);
        autoLoadSwitch->setChecked(autoLoadEarlier);
        autoLoadSwitch->blockSignals(false);
    }
}

void ConsoleWidget::UpdateInfoLabel()
{
    if (!InfoLabel || !agent)
        return;
    const QString userHost = formatAgentUserHost(agent->data);
    if (userHost.isEmpty()) {
        InfoLabel->hide();
        InfoLabel->clear();
    } else {
        QString info = QString("[%1] %2").arg(agent->data.Id).arg(userHost);
        if (!agent->data.Process.isEmpty()) {
            info += QString(" | %1 (%2%3)").arg(agent->data.Process, agent->data.Arch, agent->data.Pid.isEmpty() ? "" : ", " + agent->data.Pid);
        }
        InfoLabel->setText(info);
        InfoLabel->show();
    }
}

static QString formatAgentIoBytes(qint64 n)
{
    if (n < 1024)
        return QString("%1 B").arg(n);
    return BytesToFormat(n);
}

static QString formatAgentIoStatus(const Agent* agent)
{
    QStringList parts;
    if (agent->IoUpTotal > 0)
        parts << QString("recv %1 / %2").arg(formatAgentIoBytes(agent->IoUpFilled), formatAgentIoBytes(agent->IoUpTotal));
    if (agent->IoDownTotal > 0)
        parts << QString("send %1 / %2").arg(formatAgentIoBytes(agent->IoDownFilled), formatAgentIoBytes(agent->IoDownTotal));

    qint64 elapsed = 0;
    if (agent->IoStarted > 0)
        elapsed = QDateTime::currentSecsSinceEpoch() - agent->IoStarted;
    if (elapsed < 0)
        elapsed = 0;
    parts << FormatSecToStr(static_cast<int>(elapsed));

    if (agent->IoUpTotal > agent->IoUpFilled && elapsed >= 2 && agent->IoUpFilled > 0) {
        const double rate = double(agent->IoUpFilled) / double(elapsed);
        const int eta = static_cast<int>(double(agent->IoUpTotal - agent->IoUpFilled) / rate + 0.5);
        if (eta >= 2)
            parts << QString("~%1 left").arg(FormatSecToStr(eta));
    }
    return parts.join(QStringLiteral(" | "));
}

void ConsoleWidget::UpdateStatusLabel()
{
    if (!StatusLabel || !agent)
        return;

    if (agent->IoActive) {
        StatusLabel->setText(formatAgentIoStatus(agent));
        return;
    }

    QString lastText;
    QString statusText;

    if (!agent->data.Mark.isEmpty()) {
        statusText = agent->data.Mark;
        if (statusText == "No response")
            lastText = agent->LastMark;
        else
            lastText = UnixTimestampGlobalToStringLocalSmall(agent->data.LastTick);
    } else {
        lastText = agent->LastMark;
        if (!agent->data.Async)
            statusText = "Sync";
    }

    QString text;
    if (!lastText.isEmpty() && !statusText.isEmpty())
        text = QString("%1 | %2").arg(lastText, statusText);
    else if (!lastText.isEmpty())
        text = lastText;
    else if (!statusText.isEmpty())
        text = statusText;

    StatusLabel->setText(text);
}

void ConsoleWidget::ConsoleOutputMessage(const qint64 timestamp, const QString &taskId, const int type, const QString &message, const QString &text, const bool completed)
{
    if (!taskId.isEmpty() && completed && m_consoleTaskClosed.contains(taskId))
        return;

    if (!message.isEmpty() || !text.isEmpty() || completed) {
        const QString idPart = !taskId.isEmpty() ? taskId : QStringLiteral("local|%1").arg(timestamp);
        const QString msgKey = QStringLiteral("%1|%2|%3|%4|%5").arg(idPart).arg(type).arg(message).arg(text).arg(completed ? 1 : 0);
        if (m_consoleTaskMsgKeys.contains(msgKey)) {
            if (!taskId.isEmpty() && completed)
                m_consoleTaskClosed.insert(taskId);
            return;
        }
        m_consoleTaskMsgKeys.insert(msgKey);
    }

    const auto theme = getActiveTheme();

    QString promptTime = "";
    if (GlobalClient->settings->data.ConsoleTime)
        promptTime = UnixTimestampGlobalToStringLocal(timestamp);

    if( !message.isEmpty() ) {

        if ( !promptTime.isEmpty() )
            OutputTextEdit->appendFormatted("[" + promptTime + "] ", [&](QTextCharFormat& fmt){ fmt = theme.debug.toFormat(); });

        if (type == CONSOLE_OUT_INFO || type == CONSOLE_OUT_LOCAL_INFO)
            OutputTextEdit->appendColor("[*] ", theme.statusInfo);
        else if (type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_LOCAL_SUCCESS)
            OutputTextEdit->appendColor("[+] ", theme.statusSuccess);
        else if (type == CONSOLE_OUT_ERROR || type == CONSOLE_OUT_LOCAL_ERROR)
            OutputTextEdit->appendColor("[-] ", theme.statusError);
        else
            OutputTextEdit->appendPlain(" ");

        QString printMessage = TrimmedEnds(message);
        if ( text.isEmpty() || type == CONSOLE_OUT_LOCAL_INFO || type == CONSOLE_OUT_LOCAL_SUCCESS || type == CONSOLE_OUT_LOCAL_ERROR || type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_ERROR)
            printMessage += "\n";
        OutputTextEdit->appendPlain(printMessage);
    }

    if ( !text.isEmpty() )
        OutputTextEdit->appendPlain( TrimmedEnds(text) + "\n");

    if (completed) {
        if (!taskId.isEmpty() && m_consoleTaskClosed.contains(taskId))
            return;
        if (!taskId.isEmpty())
            m_consoleTaskClosed.insert(taskId);

        QString deleter = "\n+-------------------------------------------------------------------------------------+\n";
        if ( !taskId.isEmpty() )
            deleter = QString("\n+--- Task [%1] closed ----------------------------------------------------------+\n").arg(taskId);

        OutputTextEdit->appendFormatted(deleter, [&](QTextCharFormat& fmt){ fmt = theme.debug.toFormat(); });
    }
}

void ConsoleWidget::ConsoleOutputPrompt(const qint64 timestamp, const QString &taskId, const QString &user, const QString &commandLine) const
{
    if (!agent) return;

    if (!taskId.isEmpty()) {
        if (m_consoleTaskPrompted.contains(taskId))
            return;
        m_consoleTaskPrompted.insert(taskId);
    } else if (!commandLine.isEmpty()) {
        const QString localKey = QStringLiteral("local|%1|%2|%3").arg(timestamp).arg(user, commandLine);
        if (m_consoleTaskPrompted.contains(localKey))
            return;
        m_consoleTaskPrompted.insert(localKey);
    }

    const auto theme = getActiveTheme();

    QString promptTime = "";
    if (GlobalClient->settings->data.ConsoleTime)
        promptTime = UnixTimestampGlobalToStringLocal(timestamp);

    if ( !commandLine.isEmpty() ) {
        OutputTextEdit->appendPlain("\n");

        if ( !promptTime.isEmpty() )
            OutputTextEdit->appendFormatted("[" + promptTime + "] ", [&](QTextCharFormat& fmt){ fmt = theme.debug.toFormat(); });

        if ( !user.isEmpty() )
            OutputTextEdit->appendFormatted(user + " ", [&](QTextCharFormat& fmt){ fmt = theme.operatorStyle.toFormat(); });

        if( !taskId.isEmpty() )
            OutputTextEdit->appendFormatted("[" + taskId + "] ", [&](QTextCharFormat& fmt){ fmt = theme.task.toFormat(); });

        OutputTextEdit->appendFormatted(agent->data.Name, [&](QTextCharFormat& fmt){ fmt = theme.agent.toFormat(); });
        OutputTextEdit->appendFormatted(" " + theme.input.symbol + " ", [&](QTextCharFormat& fmt){ fmt = theme.input.style.toFormat(); });

        OutputTextEdit->appendFormatted(commandLine + "\n", [&](QTextCharFormat& fmt){ fmt = theme.command.toFormat(); });
    }
}


void ConsoleWidget::applyHistoryBarMetrics()
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

void ConsoleWidget::setHistoryBarExpanded(bool on)
{
    historyExpanded = on;
    if (historyContent)
        historyContent->setVisible(on);
    if (historyToggleBtn) {
        historyToggleBtn->setToolTip(on ? tr("Collapse history controls") : tr("History / pagination"));
    }
    applyHistoryBarMetrics();
    historyBar->adjustSize();
    positionConsoleOverlays();
}

void ConsoleWidget::applyHistoryBarStyle()
{
    if (!historyBar)
        return;
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
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

void ConsoleWidget::positionHistoryBar()
{
    if (!consoleHost || !historyBar)
        return;

    historyBar->adjustSize();
    historyBar->raise();
    historyBar->show();
}

void ConsoleWidget::positionSearchPanel()
{
    if (!consoleHost || !searchPanel || !searchPanel->isVisible())
        return;
    searchPanel->adjustSize();
    searchPanel->raise();
}

void ConsoleWidget::positionConsoleOverlays()
{
    if (!consoleHost)
        return;

    constexpr int margin = 6;
    constexpr int gap = 4;

    int sbW = 0;
    if (OutputTextEdit && OutputTextEdit->verticalScrollBar() && OutputTextEdit->verticalScrollBar()->isVisible())
        sbW = OutputTextEdit->verticalScrollBar()->width();

    const int hostW = consoleHost->width();
    const bool searchVis = searchPanel && searchPanel->isVisible();
    const bool histVis = historyBar != nullptr;

    if (searchVis) {
        searchPanel->setMaximumWidth(QWIDGETSIZE_MAX);
        searchPanel->adjustSize();
    }
    if (histVis) {
        historyBar->adjustSize();
    }

    const int histW = histVis ? historyBar->width() : 0;
    const int histH = histVis ? historyBar->height() : 0;
    const int searchW = searchVis ? searchPanel->width() : 0;
    const int searchH = searchVis ? searchPanel->height() : 0;

    const int needSideBySide = margin + searchW + gap + histW + margin + sbW;
    const bool stack = searchVis && histVis && needSideBySide > hostW;

    int histX = margin;
    int histY = margin;
    if (histVis) {
        histX = qMax(margin, hostW - histW - margin - sbW);
        histY = margin;
        historyBar->move(histX, histY);
        historyBar->show();
        historyBar->raise();
    }

    if (searchVis) {
        int searchX = margin;
        int searchY = margin;
        if (stack) {
            searchY = margin + histH + gap;
            const int maxSearchW = qMax(120, hostW - 2 * margin);
            if (searchW > maxSearchW) {
                searchPanel->setMaximumWidth(maxSearchW);
                searchPanel->adjustSize();
            }
        } else {
            const int maxAlone = qMax(120, hostW - 2 * margin);
            if (searchW > maxAlone) {
                searchPanel->setMaximumWidth(maxAlone);
                searchPanel->adjustSize();
            }
            if (histVis && searchX + searchPanel->width() + gap > histX) {
                searchY = margin + histH + gap;
            }
        }
        searchPanel->move(searchX, searchY);
        searchPanel->raise();
    }
}

bool ConsoleWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == consoleHost && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
        positionConsoleOverlays();
        refitInputHeight();
    }
    if (watched == InputLineEdit && event->type() == QEvent::Show)
        refitInputHeight();
    if (watched == InputLineEdit && event->type() == QEvent::Resize) {
        auto* resizeEvent = static_cast<QResizeEvent*>(event);
        if (resizeEvent && resizeEvent->oldSize().width() != resizeEvent->size().width())
            refitInputHeight();
    }
    if (watched == inputFrame && event->type() == QEvent::MouseButtonPress && InputLineEdit)
        InputLineEdit->setFocus();
    if (watched == searchPanel && (event->type() == QEvent::Show || event->type() == QEvent::Hide || event->type() == QEvent::Resize)) {
        QTimer::singleShot(0, this, [this]() { positionConsoleOverlays(); });
    }
    return DockTab::eventFilter(watched, event);
}

void ConsoleWidget::updateHistoryBar()
{
    const bool busy = loadingPage || loadAllPending;

    QString statusText = QStringLiteral("—");
    if (historyStatusLabel) {
        if (loadAllPending)
            statusText = tr("Loading %1 / %2…").arg(loadedItemCount).arg(totalKnown > 0 ? totalKnown : loadedItemCount);
        else if (totalKnown > 0)
            statusText = QStringLiteral("%1 / %2").arg(loadedItemCount).arg(totalKnown);
        else if (loadedItemCount > 0)
            statusText = QString::number(loadedItemCount);
        historyStatusLabel->setText(statusText);
    }

    if (historyToggleBtn && !historyExpanded) {
        historyToggleBtn->setToolTip(tr("History: %1 — click to expand").arg(statusText));
    }

    if (loadEarlierButton)
        loadEarlierButton->setEnabled(hasMore && !busy);
    if (loadAllButton) {
        loadAllButton->setEnabled(hasMore && !busy);
        loadAllButton->setVisible(!loadAllPending);
    }
    if (stopLoadButton)
        stopLoadButton->setVisible(loadAllPending);
    if (jumpLatestButton)
        jumpLatestButton->setEnabled(true);
    if (pageSizeSpin)
        pageSizeSpin->setEnabled(!busy);

    if (hasMore && totalKnown > loadedItemCount) {
        searchPanel->setScopeHint(QStringLiteral("· partial"));
    } else {
        searchPanel->setScopeHint(QString());
    }

    positionConsoleOverlays();
}

int ConsoleWidget::effectiveLoadLimit() const
{
    if (loadAllPending)
        return qBound(pageSize, 500, 2000);
    return pageSize;
}

void ConsoleWidget::finishBulkLoad()
{
    if (OutputTextEdit) {
        OutputTextEdit->setBulkInsertMode(false);
        OutputTextEdit->setUpdatesEnabled(true);
    }
}

void ConsoleWidget::applyConsolePacket(const QJsonObject& obj, bool seedHistory)
{
    int spType = obj["type"].toInt();
    if (spType == 0) spType = obj["SpType"].toInt();

    switch (spType) {
        case TYPE_AGENT_CONSOLE_OUT:
            ConsoleOutputMessage( static_cast<qint64>(obj["time"].toDouble()), "", obj["a_msg_type"].toInt(), obj["a_message"].toString(), obj["a_text"].toString(), false );
            break;
        case TYPE_AGENT_CONSOLE_LOCAL: {
            const qint64 t = static_cast<qint64>(obj["time"].toDouble());
            const QString cmdline = obj["a_cmdline"].toString();
            ConsoleOutputPrompt(t, "", "", cmdline);
            ConsoleOutputMessage(t, "", CONSOLE_OUT_LOCAL_INFO, obj["a_message"].toString(), obj["a_text"].toString(), false);
            if (seedHistory && !cmdline.isEmpty())
                AddToHistory(cmdline);
            break;
        }
        case TYPE_AGENT_CONSOLE_ERROR: {
            const qint64 t = static_cast<qint64>(obj["time"].toDouble());
            const QString cmdline = obj["a_cmdline"].toString();
            ConsoleOutputPrompt(t, "", "", cmdline);
            ConsoleOutputMessage(t, "", CONSOLE_OUT_LOCAL_ERROR, obj["a_message"].toString(), "", true);
            if (seedHistory && !cmdline.isEmpty())
                AddToHistory(cmdline);
            break;
        }
        case TYPE_AGENT_CONSOLE_TASK_SYNC: {
            qint64 startTime  = static_cast<qint64>(obj["a_start_time"].toDouble());
            qint64 finishTime = static_cast<qint64>(obj["a_finish_time"].toDouble());
            bool completed = obj["a_completed"].toBool();
            QString taskIdStr = QString::number(parseI64(obj, "a_task_id"));
            QString cmdline   = obj["a_cmdline"].toString();

            ConsoleOutputPrompt(startTime, taskIdStr, obj["a_client"].toString(), cmdline);
            ConsoleOutputMessage( completed ? finishTime : startTime, taskIdStr, obj["a_msg_type"].toInt(), obj["a_message"].toString(), obj["a_text"].toString(), completed );
            if (seedHistory && !cmdline.isEmpty())
                AddToHistory(cmdline);
            break;
        }
        case TYPE_AGENT_CONSOLE_TASK_UPD:
            ConsoleOutputMessage( static_cast<qint64>(obj["a_finish_time"].toDouble()), QString::number(parseI64(obj, "a_task_id")), obj["a_msg_type"].toInt(), obj["a_message"].toString(), obj["a_text"].toString(), obj["a_completed"].toBool() );
            break;
        default:
            break;
    }
}

void ConsoleWidget::applyPageItems(const QJsonArray& items, bool prepend)
{
    const bool seedHistory = !prepend && !historyWindow;

    if (prepend) {
        auto* sb = OutputTextEdit->verticalScrollBar();
        int oldValue = sb->value();
        int oldMax   = sb->maximum();

        OutputTextEdit->beginPrepend();
        try {
            for (const QJsonValue& v : items) {
                if (v.isObject())
                    applyConsolePacket(v.toObject(), seedHistory);
            }
        } catch (...) {}
        OutputTextEdit->endPrepend();

        int newMax = sb->maximum();
        sb->setValue(oldValue + (newMax - oldMax));
    } else {
        OutputTextEdit->setSyncMode(true);
        for (const QJsonValue& v : items) {
            if (v.isObject())
                applyConsolePacket(v.toObject(), seedHistory);
        }
        OutputTextEdit->setSyncMode(false);
    }
}

void ConsoleWidget::loadInitialPage()
{
    if (initialLoaded || loadingPage || !agent || !adaptixWidget)
        return;

    loadingPage = true;
    updateHistoryBar();

    const quint64 gen = loadGen;
    QPointer<ConsoleWidget> self = this;
    HttpReqConsoleGetPageAsync(agent->data.Id, 0, effectiveLoadLimit(), *adaptixWidget->GetProfile(),
        [self, gen](bool success, const QString&, const QJsonObject& response) {
            if (!self || self->loadGen != gen)
                return;
            self->loadingPage = false;
            if (!success) {
                self->loadAllPending = false;
                self->finishBulkLoad();
                self->updateHistoryBar();
                return;
            }
            QJsonArray items = response["items"].toArray();
            int total = response["total"].toInt();

            self->applyPageItems(items, false);

            self->initialLoaded    = true;
            self->viewCleared      = false;
            self->historyWindow    = false;
            self->totalKnown       = total;
            self->loadedItemCount  = items.size();
            self->hasMore          = response["has_more"].toBool();
            if (!items.isEmpty())
                self->oldestLoadedId = parseI64(response, "oldest_id");
            self->updateHistoryBar();

            if (self->OutputTextEdit && !self->loadAllPending) {
                auto* sb = self->OutputTextEdit->verticalScrollBar();
                sb->setValue(sb->maximum());
            }

            if (self->loadAllPending && self->hasMore) {
                QTimer::singleShot(0, self, [self, gen]() {
                    if (self && self->loadGen == gen && self->loadAllPending)
                        self->loadMorePage();
                });
            } else if (self->loadAllPending) {
                self->loadAllPending = false;
                self->finishBulkLoad();
                self->updateHistoryBar();
            }
        });
}

void ConsoleWidget::loadMorePage()
{
    if (loadingPage || !agent || !adaptixWidget)
        return;
    if (!hasMore) {
        if (loadAllPending) {
            loadAllPending = false;
            finishBulkLoad();
        }
        updateHistoryBar();
        return;
    }

    viewCleared = false;

    loadingPage = true;
    updateHistoryBar();

    const quint64 gen = loadGen;
    QPointer<ConsoleWidget> self = this;
    HttpReqConsoleGetPageAsync(agent->data.Id, oldestLoadedId, effectiveLoadLimit(), *adaptixWidget->GetProfile(),
        [self, gen](bool success, const QString&, const QJsonObject& response) {
            if (!self || self->loadGen != gen)
                return;
            self->loadingPage = false;
            if (!success) {
                self->loadAllPending = false;
                self->finishBulkLoad();
                self->updateHistoryBar();
                return;
            }
            QJsonArray items = response["items"].toArray();
            int total = response["total"].toInt();

            if (items.isEmpty()) {
                self->totalKnown = total;
                self->hasMore    = response["has_more"].toBool();
                self->loadAllPending = false;
                self->finishBulkLoad();
                self->updateHistoryBar();
                return;
            }

            self->applyPageItems(items, true);

            self->loadedItemCount += items.size();
            self->totalKnown = total;
            self->hasMore    = response["has_more"].toBool();
            const qint64 newOldest = parseI64(response, "oldest_id");
            if (newOldest > 0)
                self->oldestLoadedId = newOldest;
            self->searchPanel->clearSelections();
            self->updateHistoryBar();

            if (self->loadAllPending && self->hasMore) {
                QTimer::singleShot(0, self, [self, gen]() {
                    if (self && self->loadGen == gen && self->loadAllPending)
                        self->loadMorePage();
                });
            } else if (self->loadAllPending) {
                self->loadAllPending = false;
                self->finishBulkLoad();
                self->updateHistoryBar();
            } else if (self->autoLoadEarlier && !self->viewCleared && self->hasMore && self->OutputTextEdit) {
                QTimer::singleShot(50, self, [self, gen]() {
                    if (!self || self->loadGen != gen || !self->OutputTextEdit || self->loadingPage || !self->hasMore || self->viewCleared)
                        return;
                    auto* bar = self->OutputTextEdit->verticalScrollBar();
                    if (bar && bar->value() <= 8)
                        self->loadMorePage();
                });
            }
        });
}

void ConsoleWidget::loadAllPages()
{
    if (!agent || !adaptixWidget || loadingPage || loadAllPending)
        return;
    if (!hasMore)
        return;

    viewCleared = false;
    loadAllPending = true;
    OutputTextEdit->setUpdatesEnabled(false);
    OutputTextEdit->setBulkInsertMode(true);
    updateHistoryBar();

    if (!initialLoaded)
        loadInitialPage();
    else
        loadMorePage();
}

void ConsoleWidget::stopLoadAll()
{
    if (!loadAllPending)
        return;
    loadAllPending = false;
    finishBulkLoad();
    updateHistoryBar();
}

void ConsoleWidget::reloadLatestPage()
{
    if (!agent || !adaptixWidget)
        return;

    stopLoadAll();
    ++loadGen;
    loadingPage = false;

    resetViewState();
    hasMore         = true;
    initialLoaded   = false;
    viewCleared     = false;
    historyWindow   = false;
    updateHistoryBar();
    loadInitialPage();
}

void ConsoleWidget::jumpToLatest()
{
    if (!OutputTextEdit)
        return;

    if (historyWindow) {
        reloadLatestPage();
        return;
    }

    auto* sb = OutputTextEdit->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ConsoleWidget::openHistorySearch()
{
    if (!agent || !adaptixWidget)
        return;

    if (searchDialog) {
        searchDialog->raise();
        searchDialog->activateWindow();
        return;
    }

    searchDialog = new DialogConsoleSearch(agent->data.Id, adaptixWidget->GetProfile(), this);
    searchDialog->setInitialQuery(searchPanel ? searchPanel->currentQuery() : QString());
    connect(searchDialog, &DialogConsoleSearch::openInConsole, this, [this](qint64 centerId, int contextLimit) {
        loadAroundHit(centerId, contextLimit);
    });
    searchDialog->show();
}

void ConsoleWidget::loadAroundHit(qint64 centerId, int limit)
{
    if (loadingPage || !agent || !adaptixWidget || centerId <= 0)
        return;

    stopLoadAll();
    ++loadGen;
    const quint64 gen = loadGen;
    viewCleared   = false;
    historyWindow = true;
    loadingPage   = true;
    updateHistoryBar();

    const int lim = limit > 0 ? qBound(1, limit, 500) : pageSize;
    QPointer<ConsoleWidget> self = this;
    HttpReqConsoleGetAroundAsync(agent->data.Id, centerId, lim, *adaptixWidget->GetProfile(),
        [self, gen](bool success, const QString&, const QJsonObject& response) {
            if (!self || self->loadGen != gen)
                return;
            self->loadingPage = false;
            if (!success) {
                self->updateHistoryBar();
                return;
            }

            QJsonArray items = response["items"].toArray();
            self->resetViewState();
            self->applyPageItems(items, false);

            self->initialLoaded   = true;
            self->viewCleared     = false;
            self->historyWindow   = true;
            self->totalKnown      = response["total"].toInt();
            self->loadedItemCount = items.size();
            self->hasMore         = response["has_more"].toBool();
            self->oldestLoadedId  = parseI64(response, "oldest_id");
            self->updateHistoryBar();
        });
}


ConsoleThemeData ConsoleWidget::getActiveTheme() const
{
    if (GlobalClient->settings->data.ConsoleUseAppTheme)
        return ConsoleThemeManager::buildFromQlementine(GlobalClient->settings->data.MainTheme, GlobalClient->settings->data.ConsoleBgImagePath, GlobalClient->settings->data.ConsoleBgDimming);
    return ConsoleThemeManager::instance().theme();
}

void ConsoleWidget::applyTheme()
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
    OutputTextEdit->setConsoleBackground(bg.color, imagePath, dimming);

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& at = qs ? qs->theme() : oclero::qlementine::Theme();
    OutputTextEdit->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background-color: transparent; color: %1; border: 1px solid %2; border-radius: 4px; }")
            .arg(theme.textColor.name(), at.borderColor.name()));
    if (inputFrame) {
        inputFrame->setStyleSheet(QStringLiteral("QFrame#ConsoleInputFrame { background-color: %1; border: 1px solid %2; border-radius: 4px; }").arg(at.backgroundColorMain4.name(), at.borderColor.name()));
    }
    if (InputLineEdit) {
        InputLineEdit->setPromptColor(at.secondaryColor);
        InputLineEdit->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; color: %1; border: none; padding: 0px; }").arg(at.secondaryColor.name()));
    }
}


void ConsoleWidget::cleanupHooksOnError(const QString& hookId, const QString& handlerId, bool hasHook, bool hasHandler)
{
    if (hasHook) {
        QWriteLocker locker(&adaptixWidget->PostHooksLock);
        adaptixWidget->PostHooksJS.remove(hookId);
    }
    if (hasHandler) {
        QWriteLocker locker(&adaptixWidget->PostHandlersLock);
        adaptixWidget->PostHandlersJS.remove(handlerId);
    }
}

void ConsoleWidget::processFileUploads(const QList<QPair<QString, QString>>& fileTasks, int index,
    QJsonObject data, const QString& commandLine, bool UI,
    const QString& hookId, const QString& handlerId, bool hasHook, bool hasHandler)
{
    if (index >= fileTasks.size()) {
        QJsonDocument jsonDoc(data);
        QString commandData = jsonDoc.toJson();

        QJsonObject dataJson;
        dataJson["id"]            = agent->data.Id;
        dataJson["ui"]            = UI;
        dataJson["cmdline"]       = commandLine;
        dataJson["data"]          = commandData;
        dataJson["ax_hook_id"]    = hookId;
        dataJson["ax_handler_id"] = handlerId;
        dataJson["wait_answer"]   = false;
        QByteArray jsonData = QJsonDocument(dataJson).toJson();

        HttpReqAgentCommandAsync(jsonData, *(agent->adaptixWidget->GetProfile()));
        return;
    }

    QString argName  = fileTasks[index].first;
    QString filePath = fileTasks[index].second;
    qint64 objId = static_cast<qint64>(QRandomGenerator::global()->generate());
    if (objId == 0)
        objId = 1;

    /// 1. Get OTP asynchronously

    QPointer<ConsoleWidget> self = this;
    QJsonObject otpData;
    otpData["id"] = toJsonI64(objId);
    HttpReqGetOTPAsync("tmp_upload", otpData, *(agent->adaptixWidget->GetProfile()),
        [self, fileTasks, index, data, commandLine, UI, hookId, handlerId, hasHook, hasHandler, argName, filePath, objId]
        (bool success, const QString& message, const QJsonObject& response) mutable {

            if (!self || !self->agent) return;
            if (!success || !response.contains("ok") || !response["ok"].toBool()) {
                self->cleanupHooksOnError(hookId, handlerId, hasHook, hasHandler);
                QString errMsg = response.contains("message") ? response["message"].toString() : message;
                MessageError(errMsg.isEmpty() ? "OTP request failed" : errMsg);
                return;
            }

            QString otp  = response["message"].toString();
            QString sUrl = self->agent->adaptixWidget->GetProfile()->GetURL() + "/otp/upload/temp";

            /// 2. Stream file upload (non-blocking dialog)

            auto* uploaderDialog = new DialogUploader(sUrl, otp, filePath, self->agent->adaptixWidget->DownloadsDock);
            uploaderDialog->setAttribute(Qt::WA_DeleteOnClose);

            connect(uploaderDialog, &DialogUploader::uploadFinished, self, [self, fileTasks, index, data, commandLine, UI, hookId, handlerId, hasHook, hasHandler, argName, filePath, objId] (bool uploadSuccess) mutable {
                if (!self || !self->agent)
                    return;
                if (!uploadSuccess) {
                    self->cleanupHooksOnError(hookId, handlerId, hasHook, hasHandler);
                    return;
                }

                QJsonObject fileRef;
                fileRef["__file_ref"] = toJsonI64(objId);
                fileRef["__file_path"] = filePath;
                data[argName] = fileRef;

                self->processFileUploads(fileTasks, index + 1, data, commandLine, UI, hookId, handlerId, hasHook, hasHandler);
            });

            uploaderDialog->show();
        });
}

void ConsoleWidget::ProcessCmdResult(const QString &commandLine, const CommanderResult &cmdResult, const bool UI)
{
    if (!agent) return;

    if ( cmdResult.output ) {
        if (UI) {
            if (cmdResult.error)
                MessageError(cmdResult.message);
        }
        else {
            QString message = "";
            QString text    = "";
            int     type    = 0;

            if (cmdResult.error) {
                type    = CONSOLE_OUT_LOCAL_ERROR;
                message = cmdResult.message;
            }
            else {
                type = CONSOLE_OUT_LOCAL;
                text = cmdResult.message;
            }

            this->ConsoleOutputPrompt(0, "", "", commandLine);
            if (cmdResult.styledHelp && !cmdResult.error) {
                const auto theme = getActiveTheme();
                const QStringList lines = text.split(QLatin1Char('\n'));
                for (QString line : lines) {
                    if (line.startsWith(kHelpInactiveMarker)) {
                        line.remove(0, 1);
                        OutputTextEdit->appendFormatted(line + QLatin1Char('\n'),
                            [&](QTextCharFormat& fmt){ fmt = theme.debug.toFormat(); });
                    } else {
                        OutputTextEdit->appendPlain(line + QLatin1Char('\n'));
                    }
                }
            } else {
                this->ConsoleOutputMessage(0, "", type, message, text, true);
            }
        }
        return;
    }

    QString hookId = "";
    if (cmdResult.post_hook.isSet) {
        QWriteLocker locker(&adaptixWidget->PostHooksLock);
        hookId = GenerateRandomString(8, "hex");
        while (adaptixWidget->PostHooksJS.contains(hookId))
            hookId = GenerateRandomString(8, "hex");
        adaptixWidget->PostHooksJS[hookId] = cmdResult.post_hook;
    }

    QString handlerId = "";
    if (cmdResult.handler.isSet) {
        QWriteLocker locker(&adaptixWidget->PostHandlersLock);
        handlerId = GenerateRandomString(8, "hex");
        while (adaptixWidget->PostHandlersJS.contains(handlerId))
            handlerId = GenerateRandomString(8, "hex");
        adaptixWidget->PostHandlersJS[handlerId] = cmdResult.handler;
    }

    /// Check for __file_path markers (large files >= 3 Mb)
    QList<QPair<QString, QString>> fileTasks;
    for (auto it = cmdResult.data.begin(); it != cmdResult.data.end(); ++it) {
        if (it.value().isObject()) {
            QJsonObject obj = it.value().toObject();
            if (obj.contains("__file_path"))
                fileTasks.append({it.key(), obj["__file_path"].toString()});
        }
    }

    if (!fileTasks.isEmpty()) {
        /// Async file upload flow — non-blocking
        this->ConsoleOutputPrompt(0, "", "", commandLine);
        this->ConsoleOutputMessage(0, "", CONSOLE_OUT_LOCAL_INFO, "Uploading file(s) to server...", "", false);

        processFileUploads(fileTasks, 0, cmdResult.data, commandLine, UI,
            hookId, handlerId, cmdResult.post_hook.isSet, cmdResult.handler.isSet);
        return;
    }

    /// Standard flow for commands without large file markers
    QJsonDocument jsonDoc(cmdResult.data);
    QString commandData = jsonDoc.toJson();

    QJsonObject dataJson;
    dataJson["id"]            = agent->data.Id;
    dataJson["ui"]            = UI;
    dataJson["cmdline"]       = commandLine;
    dataJson["data"]          = commandData;
    dataJson["ax_hook_id"]    = hookId;
    dataJson["ax_handler_id"] = handlerId;
    dataJson["wait_answer"]   = false;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    /// 5 Mb fallback for non-file large commands (e.g. from scripts)
    if (commandData.size() < 0x500000) {
        HttpReqAgentCommandAsync(jsonData, *(agent->adaptixWidget->GetProfile()));
    }
    else {
        /// 1. Get OTP

        QString message = QString();
        bool ok = false;
        qint64 objId = static_cast<qint64>(QRandomGenerator::global()->generate());
        if (objId == 0)
            objId = 1;
        QJsonObject otpData;
        otpData["id"] = toJsonI64(objId);
        bool result = HttpReqGetOTP("tmp_upload", otpData, *(agent->adaptixWidget->GetProfile()), &message, &ok);
        if (!result) {
            cleanupHooksOnError(hookId, handlerId, cmdResult.post_hook.isSet, cmdResult.handler.isSet);
            MessageError("Response timeout");
            return;
        }
        if (!ok) {
            cleanupHooksOnError(hookId, handlerId, cmdResult.post_hook.isSet, cmdResult.handler.isSet);
            MessageError(message);
            return;
        }
        QString otp = message;

        /// 2. Upload with OTP

        QString sUrl = agent->adaptixWidget->GetProfile()->GetURL() + "/otp/upload/temp";

        auto* uploaderDialog = new DialogUploader(sUrl, otp, jsonData, agent->adaptixWidget->DownloadsDock);
        uploaderDialog->setAttribute(Qt::WA_DeleteOnClose);

        QPointer<ConsoleWidget> self = this;
        connect(uploaderDialog, &DialogUploader::uploadFinished, this, [self, hookId, handlerId, objId, cmdResult](const bool success) {
            if (!self || !self->agent)
                return;
            if (!success) {
                self->cleanupHooksOnError(hookId, handlerId, cmdResult.post_hook.isSet, cmdResult.handler.isSet);
                return;
            }

            /// 3. Send Command
            QJsonObject data2Json;
            data2Json["object_id"] = toJsonI64(objId);
            QByteArray json2Data = QJsonDocument(data2Json).toJson();
            HttpReqAgentCommandFileAsync(json2Data, *(self->agent->adaptixWidget->GetProfile()));
        });

        uploaderDialog->show();
    }
}

/// SLOTS

void ConsoleWidget::processInput()
{
    if (!agent || !commander)
        return;

    QString commandLine = TrimmedEnds(InputLineEdit->toPlainText());

    if ( this->userSelectedCompletion ) {
        this->userSelectedCompletion = false;
            return;
    }

    InputLineEdit->clear();
    refitInputHeight();
    if (commandLine.isEmpty())
        return;

    this->AddToHistory(commandLine);

    auto cmdResult = commander->ProcessInput( agent->data.Id, commandLine );
    if (cmdResult.is_pre_hook)
        return;

    this->ProcessCmdResult(commandLine, cmdResult, false);
}

void ConsoleWidget::handleShowHistory()
{
    if (!kphInputLineEdit)
        return;

    QDialog *historyDialog = new QDialog(this);
    historyDialog->setWindowTitle(tr("Command History"));
    historyDialog->setAttribute(Qt::WA_DeleteOnClose);


    QListWidget *historyList = new QListWidget(historyDialog);
    historyList->setWordWrap(true);
    historyList->setTextElideMode(Qt::ElideNone);
    historyList->setAlternatingRowColors(true);
    historyList->setItemDelegate(new QStyledItemDelegate(historyList));

    QPushButton *closeButton = new QPushButton(tr("Close"), historyDialog);

    QVBoxLayout *layout = new QVBoxLayout(historyDialog);
    layout->addWidget(historyList);
    layout->addWidget(closeButton);

    const QStringList& history = kphInputLineEdit->getHistory();

    for (const QString &command : history) {
        QListWidgetItem *item = new QListWidgetItem(command);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setToolTip(command);
        int lines = (command.length() / 80) + 1;
        item->setSizeHint(QSize(item->sizeHint().width(), lines * 20));
        historyList->addItem(item);
    }

    if (history.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("No command history available"));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        historyList->addItem(item);
    }

    connect(closeButton, &QPushButton::clicked, historyDialog, &QDialog::accept);

    connect(historyList, &QListWidget::itemDoubleClicked, this, [this, historyDialog](const QListWidgetItem *item) {
        InputLineEdit->setPlainText(item->text());
        refitInputHeight();
        historyDialog->accept();
        InputLineEdit->setFocus();
    });

    historyDialog->resize(800, 500);
    historyDialog->move(QCursor::pos() - QPoint(historyDialog->width()/2, historyDialog->height()/2));

    historyDialog->setModal(true);
    historyDialog->show();
}

void ConsoleWidget::handleShowHelp()
{
    if (!commander)
        return;

    const QString seed = InputLineEdit ? InputLineEdit->toPlainText() : QString();
    const qint64 agentId = agent ? agent->data.Id : 0;

    if (helpDialog) {
        helpDialog->reloadCatalog();
        helpDialog->setQuery(seed);
        helpDialog->raise();
        helpDialog->activateWindow();
        return;
    }

    helpDialog = new DialogConsoleHelp(commander, agentId, this);
    connect(helpDialog, &DialogConsoleHelp::insertCommand, this, [this](const QString& command) {
        if (InputLineEdit)
            InputLineEdit->setPlainText(command);
            refitInputHeight();
        InputFocus();
    });
    helpDialog->setQuery(seed);
    helpDialog->move(QCursor::pos() - QPoint(helpDialog->width() / 2, helpDialog->height() / 2));
    helpDialog->show();
}

void ConsoleWidget::onCompletionSelected(const QString &selectedText)
{
    if (!InputLineEdit)
        return;
    InputLineEdit->blockSignals(true);
    InputLineEdit->setPlainText(selectedText);
    QTextCursor cursor = InputLineEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    InputLineEdit->setTextCursor(cursor);
    InputLineEdit->blockSignals(false);
    refitInputHeight();
}
