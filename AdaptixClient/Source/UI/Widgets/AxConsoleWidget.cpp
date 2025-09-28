#include <QJSEngine>
#include <QJSValue>
#include <UI/Widgets/AxConsoleWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/KeyPressHandler.h>
#include <Utils/CustomElements.h>
#include <Utils/FontManager.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>

AxConsoleWidget::AxConsoleWidget(AxScriptManager* m, AdaptixWidget* w) : DockTab("AxScript Console", w->GetProfile()->GetProject(), ":/icons/code_blocks"), adaptixWidget(w), scriptManager(m)
{
    this->createUI();

    connect(InputLineEdit,  &QLineEdit::returnPressed,     this, &AxConsoleWidget::processInput, Qt::QueuedConnection );
    connect(searchLineEdit, &QLineEdit::returnPressed,     this, &AxConsoleWidget::handleSearch);
    connect(nextButton,     &ClickableLabel::clicked,      this, &AxConsoleWidget::handleSearch);
    connect(prevButton,     &ClickableLabel::clicked,      this, &AxConsoleWidget::handleSearchBackward);
    connect(hideButton,     &ClickableLabel::clicked,      this, &AxConsoleWidget::toggleSearchPanel);
    connect(OutputTextEdit, &TextEditConsole::ctx_find,    this, &AxConsoleWidget::toggleSearchPanel);
    connect(OutputTextEdit, &TextEditConsole::ctx_history, this, &AxConsoleWidget::handleShowHistory);
    connect(ResetButton,    &QPushButton::clicked,         this, &AxConsoleWidget::onResetScript);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &AxConsoleWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, OutputTextEdit, &QTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, OutputTextEdit, &QTextEdit::selectAll);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+H"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &AxConsoleWidget::handleShowHistory);

    kphInputLineEdit = new KPH_ConsoleInput(InputLineEdit, OutputTextEdit, this);
    InputLineEdit->installEventFilter(kphInputLineEdit);

    this->dockWidget->setWidget(this);
}

AxConsoleWidget::~AxConsoleWidget() {}

void AxConsoleWidget::createUI()
{
    searchWidget = new QWidget(this);

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

    OutputTextEdit = new TextEditConsole(this, 30000, true, true);
    OutputTextEdit->setReadOnly(true);
    OutputTextEdit->setProperty( "TextEditStyle", "console" );
    OutputTextEdit->setFont( FontManager::instance().getFont("Hack") );

    CmdLabel = new QLabel( "ax >", this );
    CmdLabel->setProperty( "LabelStyle", "console" );

    InputLineEdit = new QLineEdit(this);
    InputLineEdit->setProperty( "LineEditStyle", "console" );
    InputLineEdit->setFont( FontManager::instance().getFont("Hack") );

    ResetButton = new QPushButton("Reset AxScript");

    MainGridLayout = new QGridLayout(this );
    MainGridLayout->setVerticalSpacing(4 );
    MainGridLayout->setContentsMargins(0, 1, 0, 4 );
    MainGridLayout->addWidget( searchWidget,   0, 0, 1, 3 );
    MainGridLayout->addWidget( OutputTextEdit, 1, 0, 1, 3 );
    MainGridLayout->addWidget( CmdLabel,       2, 0, 1, 1 );
    MainGridLayout->addWidget( InputLineEdit,  2, 1, 1, 1 );
    MainGridLayout->addWidget( ResetButton,    2, 2, 1, 1 );

    searchWidget->setVisible(false);
}

void AxConsoleWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(OutputTextEdit->document());
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat baseFmt;
    baseFmt.setBackground(Qt::blue);
    baseFmt.setForeground(Qt::white);

    while (true) {
        auto found = OutputTextEdit->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    OutputTextEdit->setExtraSelections(allSelections);
}

void AxConsoleWidget::highlightCurrent() const
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

    OutputTextEdit->setExtraSelections(sels);

    OutputTextEdit->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}

void AxConsoleWidget::OutputClear() const { OutputTextEdit->clear(); }

void AxConsoleWidget::InputFocus() const { InputLineEdit->setFocus(); }

void AxConsoleWidget::AddToHistory(const QString &command) { kphInputLineEdit->AddToHistory(command); }

void AxConsoleWidget::PrintMessage(const QString &message) { OutputTextEdit->appendColor(message + "\n", QColor(COLOR_ConsoleWhite)); }

void AxConsoleWidget::PrintError(const QString &message) { OutputTextEdit->appendColor(message + "\n", QColor(COLOR_ChiliPepper)); }

void AxConsoleWidget::processInput()
{
    QString code = TrimmedEnds(InputLineEdit->text());
    if (code.isEmpty())
        return;

    InputLineEdit->clear();
    if (code.isEmpty())
        return;

    this->AddToHistory(code);

    OutputTextEdit->appendColorUnderline("ax script", QColor(COLOR_LightGray));
    OutputTextEdit->appendColor(" >>> ", QColor(COLOR_LightGray));
    OutputTextEdit->appendColorBold(code + "\n", QColor(COLOR_White));

    QJSValue result = scriptManager->MainScriptEngine()->evaluate(code);
    if (result.isError()) {
        QString errorString = QString("%1\n").arg(result.toString());
        OutputTextEdit->appendColor(errorString, QColor(COLOR_ChiliPepper));
    }
    else if (!result.isUndefined()) {
        QString message = result.toString();
        if (!message.isEmpty())
            OutputTextEdit->appendColor(message + "\n", QColor(COLOR_ConsoleWhite));
    }
}

void AxConsoleWidget::toggleSearchPanel()
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

void AxConsoleWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if ( pattern.isEmpty() && allSelections.size() ) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        OutputTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText().compare( pattern, Qt::CaseInsensitive) != 0 ) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    }
    else {
        currentIndex = (currentIndex + 1) % allSelections.size();
    }

    highlightCurrent();
}

void AxConsoleWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        OutputTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText().compare( pattern, Qt::CaseInsensitive) != 0 ) {
        findAndHighlightAll(pattern);
        currentIndex = allSelections.size() - 1;
    }
    else {
        currentIndex = (currentIndex - 1 + allSelections.size()) % allSelections.size();
    }

    highlightCurrent();
}

void AxConsoleWidget::handleShowHistory()
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
        InputLineEdit->setText(item->text());
        historyDialog->accept();
        InputLineEdit->setFocus();
    });

    historyDialog->resize(800, 500);
    historyDialog->move(QCursor::pos() - QPoint(historyDialog->width()/2, historyDialog->height()/2));

    historyDialog->setModal(true);
    historyDialog->show();
}

void AxConsoleWidget::onResetScript()
{
    scriptManager->ResetMain();
    OutputTextEdit->clear();
}
