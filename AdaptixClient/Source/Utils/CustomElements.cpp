#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QTextBlock>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>



CardListWidget::CardListWidget(QWidget *parent) : QListWidget(parent)
{
    setMouseTracking(true);
    setSpacing(1);
    setItemDelegate(new CardListDelegate(this));
}

void CardListWidget::addCard(const QString &title, const QString &text)
{
    auto* item = new QListWidgetItem(this);
    item->setData(TitleRole, title);
    item->setData(TextRole, text);
}

CardListDelegate::CardListDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void CardListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    QString title = index.data(CardListWidget::TitleRole).toString();
    QString text = index.data(CardListWidget::TextRole).toString();

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;

    auto* cardList = qobject_cast<const CardListWidget*>(option.widget);

    constexpr int cardMargin = 3;
    QRect cardRect = option.rect.adjusted(cardMargin, 2, -cardMargin, -2);

    QColor bgColor, titleColor, subtitleColor;

    if (cardList) {
        if (isSelected) {
            bgColor = cardList->itemBackgroundSelected();
            titleColor = cardList->titleColorSelected();
            subtitleColor = cardList->subtitleColorSelected();
        } else if (isHovered) {
            bgColor = cardList->itemBackgroundHover();
            titleColor = cardList->titleColor();
            subtitleColor = cardList->subtitleColor();
        } else {
            bgColor = cardList->itemBackground();
            titleColor = cardList->titleColor();
            subtitleColor = cardList->subtitleColor();
        }
    } else {
        bgColor = isSelected ? option.palette.highlight().color() : option.palette.mid().color();
        if (!isSelected) bgColor.setAlpha(40);
        titleColor = isSelected ? option.palette.highlightedText().color() : option.palette.text().color();
        subtitleColor = titleColor;
        subtitleColor.setAlpha(isSelected ? 200 : 140);
    }

    painter->fillRect(cardRect, bgColor);

    constexpr int hPadding = 12;
    constexpr int vPadding = 10;
    constexpr int lineSpacing = 4;

    QRect contentRect = cardRect.adjusted(hPadding, vPadding, -hPadding, -vPadding);

    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Bold);
    QFontMetrics titleFm(titleFont);

    painter->setFont(titleFont);
    painter->setPen(titleColor);

    QRect titleRect = contentRect;
    titleRect.setHeight(titleFm.height());
    QString elidedTitle = titleFm.elidedText(title, Qt::ElideRight, titleRect.width());
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    if (!text.isEmpty()) {
        QFont subtitleFont = option.font;
        int subtitleSize = qMax(subtitleFont.pointSize() - 1, 8);
        subtitleFont.setPointSize(subtitleSize);
        QFontMetrics subtitleFm(subtitleFont);

        painter->setFont(subtitleFont);
        painter->setPen(subtitleColor);

        QRect subtitleRect = contentRect;
        subtitleRect.setTop(titleRect.bottom() + lineSpacing);
        subtitleRect.setHeight(subtitleFm.height());
        QString elidedSubtitle = subtitleFm.elidedText(text, Qt::ElideRight, subtitleRect.width());
        painter->drawText(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedSubtitle);
    }

    painter->restore();
}

QSize CardListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)

    constexpr int cardMargin = 3;
    constexpr int vPadding = 10;
    constexpr int lineSpacing = 4;

    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Medium);
    QFontMetrics titleFm(titleFont);

    QFont subtitleFont = option.font;
    int subtitleSize = qMax(subtitleFont.pointSize() - 1, 8);
    subtitleFont.setPointSize(subtitleSize);
    QFontMetrics subtitleFm(subtitleFont);

    int totalHeight = cardMargin + vPadding + titleFm.height() + lineSpacing + subtitleFm.height() + vPadding + cardMargin;

    return QSize(190, totalHeight);
}



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





TextEditConsole::TextEditConsole(QWidget* parent, int maxLines, bool noWrap, bool autoScroll) : QTextEdit(parent), cachedCursor(this->textCursor()), maxLines(maxLines), noWrap(noWrap), autoScroll(autoScroll)
{
    cachedCursor.movePosition(QTextCursor::End);

    if (noWrap)
        setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &TextEditConsole::customContextMenuRequested, this, &TextEditConsole::createContextMenu);

    batchMutex = new QMutex();
    batchTimer = new QTimer(this);
    batchTimer->setSingleShot(true);
    batchTimer->setInterval(BATCH_INTERVAL_MS);
    connect(batchTimer, &QTimer::timeout, this, &TextEditConsole::flushPendingText);
}

void TextEditConsole::createContextMenu(const QPoint &pos) {
    QMenu *menu = new QMenu(this);
    
    QAction *copyAction = menu->addAction("Copy         (Ctrl + C)");
    connect(copyAction, &QAction::triggered, this, [this]() { copy(); });
    
    QAction *selectAllAction = menu->addAction("Select All   (Ctrl + A)");
    connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    QAction *findAction = menu->addAction("Find         (Ctrl + F)");
    connect(findAction, &QAction::triggered, this, [this]() { Q_EMIT ctx_find(); });
    
    QAction *clearAction = menu->addAction("Clear        (Ctrl + L)");
    connect(clearAction, &QAction::triggered, this, [this]() { clear(); });

    menu->addSeparator();

    QAction *showHistory = menu->addAction("Show history (Ctrl + H)");
    connect(showHistory, &QAction::triggered, this, [this]() { Q_EMIT ctx_history(); });

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
    QMutexLocker locker(batchMutex);
    pendingText += text;

    if (pendingText.size() >= MAX_BATCH_SIZE) {
        locker.unlock();
        flushPendingText();
    } else if (!batchTimer->isActive()) {
        batchTimer->start();
    }
}

void TextEditConsole::flushPendingText()
{
    QMutexLocker locker(batchMutex);
    if (pendingText.isEmpty())
        return;

    QString textToAppend = pendingText;
    pendingText.clear();
    locker.unlock();

    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();

    cachedCursor.movePosition(QTextCursor::End);
    cachedCursor.insertText(textToAppend, QTextCharFormat());

    auto doc = this->document();
    int currentLines = doc->blockCount();
    int trimThreshold = static_cast<int>(maxLines * 1.5);

    if (currentLines > trimThreshold) {
        trimExcessLines();
        appendCount = 0;
    } else {
        appendCount++;
        if (appendCount >= 200 && currentLines > static_cast<int>(maxLines * 0.9)) {
            trimExcessLines();
            appendCount = 0;
        }
    }

    if (autoScroll || atBottom)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TextEditConsole::appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn)
{
    flushPendingText();

    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();

    cachedCursor.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    styleFn(fmt);
    cachedCursor.insertText(text, fmt);

    appendCount++;
    auto doc = this->document();
    int currentLines = doc->blockCount();
    int trimThreshold = static_cast<int>(maxLines * 1.5);

    if (currentLines > trimThreshold) {
        trimExcessLines();
        appendCount = 0;
    } else if (appendCount >= 200 && currentLines > static_cast<int>(maxLines * 0.9)) {
        trimExcessLines();
        appendCount = 0;
    }

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
    int blockCount = doc->blockCount();
    if (blockCount <= maxLines)
        return;

    int linesToRemove = blockCount - maxLines;

    QTextCursor c(doc);
    c.movePosition(QTextCursor::Start);

    QTextBlock keepFromBlock = doc->findBlockByNumber(maxLines);
    if (keepFromBlock.isValid()) {
        c.movePosition(QTextCursor::Start);
        c.setPosition(keepFromBlock.position(), QTextCursor::KeepAnchor);
        c.removeSelectedText();

        doc = this->document();
        if (doc->blockCount() > maxLines) {
            static int recursionDepth = 0;
            if (recursionDepth < 3) {
                recursionDepth++;
                trimExcessLines();
                recursionDepth--;
            }
        }
    } else {
        while (doc->blockCount() > maxLines && linesToRemove > 0) {
            QTextBlock block = doc->firstBlock();
            if (!block.isValid())
                break;

            QTextCursor c(block);
            c.select(QTextCursor::BlockUnderCursor);
            c.removeSelectedText();
            c.deleteChar();
            linesToRemove--;

            if (linesToRemove > 5000)
                break;
        }
    }
}