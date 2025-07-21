#include <UI/Widgets/LogsWidget.h>
#include <Utils/Convert.h>

LogsWidget::LogsWidget()
{
    this->createUI();

    connect(searchLineEdit,      &QLineEdit::returnPressed,  this, &LogsWidget::handleSearch);
    connect(nextButton,          &ClickableLabel::clicked,   this, &LogsWidget::handleSearch);
    connect(prevButton,          &ClickableLabel::clicked,   this, &LogsWidget::handleSearchBackward);
    connect(hideButton,          &ClickableLabel::clicked,   this, &LogsWidget::toggleSearchPanel);
    connect(logsConsoleTextEdit, &TextEditConsole::ctx_find, this, &LogsWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), logsConsoleTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &LogsWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), logsConsoleTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, logsConsoleTextEdit, &QTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), logsConsoleTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, logsConsoleTextEdit, &QTextEdit::selectAll);
}

LogsWidget::~LogsWidget() = default;

void LogsWidget::createUI()
{
    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    prevButton = new ClickableLabel("<");
    prevButton->setCursor( Qt::PointingHandCursor );

    nextButton = new ClickableLabel(">");
    nextButton->setCursor( Qt::PointingHandCursor );

    searchLabel    = new QLabel("0 of 0");
    searchLineEdit = new QLineEdit();
    searchLineEdit->setPlaceholderText("Find");
    searchLineEdit->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor( Qt::PointingHandCursor );

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 3, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(prevButton);
    searchLayout->addWidget(nextButton);
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(spacer);

    logsConsoleTextEdit = new TextEditConsole(this);
    logsConsoleTextEdit->setReadOnly(true);
    logsConsoleTextEdit->setProperty("TextEditStyle", "console" );

    logsGridLayout = new QGridLayout(this);
    logsGridLayout->setContentsMargins(1, 1, 1, 1);
    logsGridLayout->setVerticalSpacing(1);
    logsGridLayout->addWidget( searchWidget,        0, 0, 1, 1);
    logsGridLayout->addWidget( logsConsoleTextEdit, 1, 0, 1, 1);

    logsWidget = new QWidget(this);
    logsWidget->setLayout(logsGridLayout);



    /// ToDo: todo list + sync chat
    todoLabel = new QLabel(this);
    todoLabel->setText("ToDo notes");
    todoLabel->setAlignment(Qt::AlignCenter);

    todoGridLayout = new QGridLayout(this);
    todoGridLayout->setContentsMargins(1, 1, 1, 1);
    todoGridLayout->setVerticalSpacing(1);
    todoGridLayout->setHorizontalSpacing(2);

    todoGridLayout->addWidget(todoLabel, 0, 0, 1, 1);

    todoWidget = new QWidget(this);
    todoWidget->setLayout(todoGridLayout);



    mainHSplitter = new QSplitter( Qt::Horizontal, this );
    mainHSplitter->setHandleWidth(3);
    mainHSplitter->addWidget(logsWidget);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->addWidget( mainHSplitter, 0, 0, 1, 1);

    this->setLayout( mainGridLayout );
}

void LogsWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(logsConsoleTextEdit->document());
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat baseFmt;
    baseFmt.setBackground(Qt::blue);
    baseFmt.setForeground(Qt::white);

    while (true) {
        auto found = logsConsoleTextEdit->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    logsConsoleTextEdit->setExtraSelections(allSelections);
}

void LogsWidget::highlightCurrent() const
{
    if (allSelections.isEmpty()) {
        searchLabel->setText("0 of 0");
        return;
    }

    auto sels = allSelections;

    QTextCharFormat activeFmt;
    activeFmt.setBackground(Qt::white);
    activeFmt.setForeground(Qt::black);

    sels[currentIndex].format = activeFmt;

    logsConsoleTextEdit->setExtraSelections(sels);

    logsConsoleTextEdit->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}

void LogsWidget::AddLogs(const int type, const qint64 time, const QString &message ) const
{
    QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString log = QString("[%1] -> ").arg(sTime);

    logsConsoleTextEdit->appendPlain(log);

    if( type == EVENT_CLIENT_CONNECT )           logsConsoleTextEdit->appendColor(message, QColor(COLOR_ConsoleWhite));
    else if( type == EVENT_CLIENT_DISCONNECT )   logsConsoleTextEdit->appendColor(message, QColor(COLOR_Gray));
    else if( type == EVENT_LISTENER_START )      logsConsoleTextEdit->appendColor(message, QColor(COLOR_BrightOrange));
    else if( type == EVENT_LISTENER_STOP )       logsConsoleTextEdit->appendColor(message, QColor(COLOR_BrightOrange));
    else if( type == EVENT_AGENT_NEW )           logsConsoleTextEdit->appendColor(message, QColor(COLOR_NeonGreen));
    else if( type == EVENT_TUNNEL_START )        logsConsoleTextEdit->appendColor(message, QColor(COLOR_PastelYellow));
    else if( type == EVENT_TUNNEL_STOP )         logsConsoleTextEdit->appendColor(message, QColor(COLOR_PastelYellow));
    else                                         logsConsoleTextEdit->appendPlain(message);

    logsConsoleTextEdit->appendPlain("\n");
}

void LogsWidget::Clear() const
{
    logsConsoleTextEdit->clear();
}

void LogsWidget::toggleSearchPanel()
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        searchLineEdit->setText("");
        handleSearch();
    }
    else {
        this->searchWidget->setVisible(true);
        searchLineEdit->setFocus();
        searchLineEdit->selectAll();
    }
}

void LogsWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if ( pattern.isEmpty() && allSelections.size() ) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        logsConsoleTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    }
    else {
        currentIndex = (currentIndex + 1) % allSelections.size();
    }

    highlightCurrent();
}

void LogsWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        logsConsoleTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = allSelections.size() - 1;
    }
    else {
        currentIndex = (currentIndex - 1 + allSelections.size()) % allSelections.size();
    }

    highlightCurrent();
}