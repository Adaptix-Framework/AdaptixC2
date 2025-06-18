#include <Utils/CustomElements.h>
#include <QTextBlock>

SpinTable::SpinTable(int rows, int columns, QWidget* parent)
{
    this->setParent(parent);

    buttonAdd = new QPushButton("Add");
    buttonAdd->setProperty("ButtonStyle", "dialog");

    buttonClear = new QPushButton("Clear");
    buttonClear->setProperty("ButtonStyle", "dialog");

    table = new QTableWidget(rows, columns, this);
    table->setAutoFillBackground( false );
    table->setShowGrid( false );
    table->setSortingEnabled( true );
    table->setWordWrap( true );
    table->setCornerButtonEnabled( false );
    table->setSelectionBehavior( QAbstractItemView::SelectRows );
    table->setSelectionMode( QAbstractItemView::SingleSelection );
    table->setFocusPolicy( Qt::NoFocus );
    table->setAlternatingRowColors( true );
    table->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    table->horizontalHeader()->setCascadingSectionResizes( true );
    table->horizontalHeader()->setHighlightSections( false );
    table->verticalHeader()->setVisible( false );

    layout = new QGridLayout( this );
    layout->addWidget(table, 0, 0, 1, 2);
    layout->addWidget(buttonAdd, 1, 0, 1, 1);
    layout->addWidget(buttonClear, 1, 1, 1, 1);

    this->setLayout(layout);

    QObject::connect(buttonAdd, &QPushButton::clicked, this, [&]()
    {
        if (table->rowCount() < 1 )
            table->setRowCount(1 );
        else
            table->setRowCount(table->rowCount() + 1 );

        table->setItem(table->rowCount() - 1, 0, new QTableWidgetItem() );
        table->selectRow(table->rowCount() - 1 );
    } );

    QObject::connect(buttonClear, &QPushButton::clicked, this, [&](){ table->setRowCount(0); } );
}



FileSelector::FileSelector(QWidget* parent) : QWidget(parent)
{
    input = new QLineEdit(this);
    input->setReadOnly(true);

    button = new QPushButton(this);
    button->setIcon(QIcon::fromTheme("folder"));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(input);
    layout->addWidget(button);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    connect(button, &QPushButton::clicked, this, [&]()
    {
        QString selectedFile = QFileDialog::getOpenFileName(this, "Select a file");
        if (selectedFile.isEmpty())
            return;

        QString filePath = selectedFile;
        input->setText(filePath);

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        QByteArray fileData = file.readAll();
        file.close();

        content = QString::fromUtf8(fileData.toBase64());
    } );
}






TextEditConsole::TextEditConsole(QWidget* parent, int maxLines, bool noWrap, bool autoScroll) : QTextEdit(parent), cachedCursor(this->textCursor()), maxLines(maxLines), noWrap(noWrap), autoScroll(autoScroll)
{
    cachedCursor.movePosition(QTextCursor::End);

    if (noWrap)
        setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &TextEditConsole::customContextMenuRequested, this, &TextEditConsole::createContextMenu);
}

void TextEditConsole::createContextMenu(const QPoint &pos) {
    QMenu *menu = new QMenu(this);
    
    QAction *copyAction = menu->addAction("Copy         (Ctrl + C)");
    connect(copyAction, &QAction::triggered, this, [this]() { copy(); });
    
    QAction *selectAllAction = menu->addAction("Select All   (Ctrl + A)");
    connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    QAction *findAction = menu->addAction("Find         (Ctrl + F)");
    connect(findAction, &QAction::triggered, this, [this]() { emit ctx_find(); });
    
    QAction *clearAction = menu->addAction("Clear        (Ctrl + L)");
    connect(clearAction, &QAction::triggered, this, [this]() { clear(); });

    menu->addSeparator();

    QAction *showHistory = menu->addAction("Show history (Ctrl + H)");
    connect(showHistory, &QAction::triggered, this, [this]() { emit ctx_history(); });

    QAction *setBufferSizeAction = menu->addAction("Set buffer size...");
    connect(setBufferSizeAction, &QAction::triggered, this, [this]() {
        bool ok;
        int newSize = QInputDialog::getInt(this, "Set buffer size", "Enter maximum number of lines:", maxLines, 100, 100000, 100, &ok);
        if (ok)
            setBufferSize(newSize);
    });
    
    QAction *noWrapAction = menu->addAction("No Wrap");
    noWrapAction->setCheckable(true);
    noWrapAction->setChecked(noWrap);
    connect(noWrapAction, &QAction::toggled, this, [this](bool checked) {
        noWrap = checked;
        setLineWrapMode(checked ? QTextEdit::NoWrap : QTextEdit::WidgetWidth);
    });
    
    QAction *autoScrollAction = menu->addAction("Auto scroll");
    autoScrollAction->setCheckable(true);
    autoScrollAction->setChecked(autoScroll);
    connect(autoScrollAction, &QAction::toggled, this, &TextEditConsole::setAutoScrollEnabled);
    
    menu->exec(mapToGlobal(pos));
    delete menu;
}

void TextEditConsole::setMaxLines(const int lines)
{
    maxLines = lines;
    trimExcessLines();
}

void TextEditConsole::setBufferSize(const int size) {
    if (size > 0) {
        maxLines = size;
        trimExcessLines();
    }
}

void TextEditConsole::setAutoScrollEnabled(const bool enabled) {
    autoScroll = enabled;
    if (enabled)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

bool TextEditConsole::isAutoScrollEnabled() const {
    return autoScroll;
}

bool TextEditConsole::isNoWrapEnabled() const {
    return noWrap;
}

void TextEditConsole::appendPlain(const QString& text)
{
    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    
    cachedCursor.movePosition(QTextCursor::End);
    cachedCursor.insertText(text, QTextCharFormat());
    
    trimExcessLines();

    if (autoScroll || atBottom)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TextEditConsole::appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn)
{
    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();

    cachedCursor.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    styleFn(fmt);
    cachedCursor.insertText(text, fmt);

    trimExcessLines();

    if (autoScroll || atBottom)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TextEditConsole::appendColor(const QString& text, const QColor color) {
    appendFormatted(text, [=](QTextCharFormat& fmt) { fmt.setForeground(color); });
}

void TextEditConsole::appendBold(const QString& text) {
    appendFormatted(text, [](QTextCharFormat& fmt) { fmt.setFontWeight(QFont::Bold); });
}

void TextEditConsole::appendUnderline(const QString& text) {
    appendFormatted(text, [](QTextCharFormat& fmt) { fmt.setFontUnderline(true); });
}

void TextEditConsole::appendColorBold(const QString& text, const QColor color) {
    appendFormatted(text, [=](QTextCharFormat& fmt) {
        fmt.setForeground(color);
        fmt.setFontWeight(QFont::Bold);
    });
}

void TextEditConsole::appendColorUnderline(const QString &text, const QColor color) {
    appendFormatted(text, [=](QTextCharFormat& fmt) {
        fmt.setForeground(color);
        fmt.setFontUnderline(true);
    });
}

void TextEditConsole::trimExcessLines() {
    auto doc = this->document();
    while (doc->blockCount() > maxLines) {
        QTextBlock block = doc->firstBlock();
        QTextCursor c(block);
        c.select(QTextCursor::BlockUnderCursor);
        c.removeSelectedText();
        c.deleteChar();
    }
}