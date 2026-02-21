#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QTextBlock>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>


VerticalTabBar::VerticalTabBar(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFixedWidth(m_tabWidth);
}

int VerticalTabBar::addTab(const QString &text)
{
    m_tabs.append({text});
    if (m_currentIndex < 0)
        setCurrentIndex(0);
    update();
    return m_tabs.size() - 1;
}

void VerticalTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs.removeAt(index);
    if (m_currentIndex >= m_tabs.size())
        setCurrentIndex(m_tabs.size() - 1);
    update();
}

void VerticalTabBar::setCurrentIndex(int index)
{
    if (index == m_currentIndex || index < 0 || index >= m_tabs.size()) return;
    m_currentIndex = index;
    Q_EMIT currentChanged(index);
    update();
}

QString VerticalTabBar::tabText(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return QString();
    return m_tabs[index].text;
}

int VerticalTabBar::tabAt(const QPoint &pos) const
{
    int y = pos.y();
    int offset = m_showAddButton ? m_tabHeight : 0;
    if (y < offset) return -1;
    int index = (y - offset) / m_tabHeight;
    if (index >= 0 && index < m_tabs.size())
        return index;
    return -1;
}

QRect VerticalTabBar::closeButtonRect(int index) const
{
    int offset = m_showAddButton ? m_tabHeight : 0;
    int y = offset + index * m_tabHeight;
    return QRect(m_tabWidth - 14, y + 4, 12, 12);
}

QRect VerticalTabBar::addButtonRect() const
{
    return QRect(0, 0, m_tabWidth, m_tabHeight);
}

void VerticalTabBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    const QPalette &pal = palette();
    QColor bgNormal = pal.color(QPalette::Window);
    QColor bgHover = pal.color(QPalette::Mid);
    QColor bgSelected = pal.color(QPalette::Highlight);
    QColor textNormal = pal.color(QPalette::WindowText);
    QColor textSelected = pal.color(QPalette::HighlightedText);
    QColor borderColor = pal.color(QPalette::Dark);

    int yOffset = 0;

    if (m_showAddButton) {
        QRect addRect = addButtonRect();
        if (m_addButtonHovered) {
            p.fillRect(addRect, bgHover);
        }
        p.setPen(textNormal);
        QFont f = font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 2);
        p.setFont(f);
        p.drawText(addRect, Qt::AlignCenter, "+");
        p.setPen(borderColor);
        p.drawLine(addRect.bottomLeft(), addRect.bottomRight());
        yOffset = m_tabHeight;
    }

    for (int i = 0; i < m_tabs.size(); ++i) {
        QRect tabRect(0, yOffset + i * m_tabHeight, m_tabWidth, m_tabHeight);
        bool selected = (i == m_currentIndex);
        bool hovered = (i == m_hoveredIndex);

        if (selected) {
            p.fillRect(tabRect, bgSelected);
        } else if (hovered) {
            p.fillRect(tabRect, bgHover);
        }

        if (selected) {
            p.fillRect(0, tabRect.top(), 3, tabRect.height(), pal.color(QPalette::Highlight));
        }

        p.setPen(selected ? textSelected : textNormal);
        QFont f = font();
        f.setBold(selected);
        p.setFont(f);
        QString num = QString::number(i + 1);
        p.drawText(tabRect, Qt::AlignCenter, num);

        if (m_closable && (selected || hovered)) {
            QRect closeRect = closeButtonRect(i);
            if (m_hoveredCloseButton == i) {
                p.setBrush(QColor(200, 60, 60));
                p.setPen(Qt::NoPen);
                p.drawEllipse(closeRect);
                p.setPen(Qt::white);
            } else {
                p.setPen(selected ? textSelected : textNormal);
            }
            p.drawText(closeRect, Qt::AlignCenter, "×");
        }

        if (i < m_tabs.size() - 1) {
            p.setPen(borderColor);
            p.drawLine(tabRect.bottomLeft(), tabRect.bottomRight());
        }
    }
}

void VerticalTabBar::mousePressEvent(QMouseEvent *event)
{
    if (m_showAddButton && addButtonRect().contains(event->pos())) {
        Q_EMIT addTabRequested();
        return;
    }
    int index = tabAt(event->pos());
    if (index >= 0) {
        if (m_closable && closeButtonRect(index).contains(event->pos())) {
            Q_EMIT tabCloseRequested(index);
        } else {
            setCurrentIndex(index);
        }
    }
}

void VerticalTabBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHover = m_hoveredIndex;
    int oldCloseHover = m_hoveredCloseButton;
    bool oldAddHover = m_addButtonHovered;
    
    m_addButtonHovered = m_showAddButton && addButtonRect().contains(event->pos());
    m_hoveredIndex = tabAt(event->pos());
    m_hoveredCloseButton = (m_closable && m_hoveredIndex >= 0 && closeButtonRect(m_hoveredIndex).contains(event->pos())) ? m_hoveredIndex : -1;
    
    if (oldHover != m_hoveredIndex || oldCloseHover != m_hoveredCloseButton || oldAddHover != m_addButtonHovered)
        update();
}

void VerticalTabBar::leaveEvent(QEvent *)
{
    m_hoveredIndex = -1;
    m_hoveredCloseButton = -1;
    m_addButtonHovered = false;
    update();
}

QSize VerticalTabBar::sizeHint() const
{
    int addButtonHeight = m_showAddButton ? m_tabHeight : 0;
    return QSize(m_tabWidth, addButtonHeight + m_tabs.size() * m_tabHeight);
}

VerticalTabWidget::VerticalTabWidget(QWidget *parent) : QWidget(parent)
{
    m_tabBar = new VerticalTabBar(this);
    m_stack = new QStackedWidget(this);
    
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack, 1);
    
    connect(m_tabBar, &VerticalTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        Q_EMIT currentChanged(index);
    });
    connect(m_tabBar, &VerticalTabBar::tabCloseRequested, this, &VerticalTabWidget::tabCloseRequested);
}

int VerticalTabWidget::addTab(QWidget *widget, const QString &label)
{
    int index = m_tabBar->addTab(label);
    m_stack->insertWidget(index, widget);
    return index;
}

void VerticalTabWidget::removeTab(int index)
{
    QWidget *w = m_stack->widget(index);
    m_stack->removeWidget(w);
    m_tabBar->removeTab(index);
}

void VerticalTabWidget::setCurrentIndex(int index)
{
    m_tabBar->setCurrentIndex(index);
}

QWidget *VerticalTabWidget::widget(int index) const
{
    return m_stack->widget(index);
}

void VerticalTabWidget::setCornerWidget(QWidget *widget)
{
    if (m_cornerWidget) {
        layout()->removeWidget(m_cornerWidget);
    }
    m_cornerWidget = widget;
    if (widget) {
        static_cast<QHBoxLayout*>(layout())->insertWidget(0, widget);
    }
}


BoldHeaderView::BoldHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setDefaultAlignment(Qt::AlignCenter);
    setMinimumSectionSize(24);
    setDefaultSectionSize(100);
    setSectionsClickable(true);
    setSortIndicatorShown(true);
}

void BoldHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPalette &pal = palette();
    QColor bgColor = pal.color(QPalette::Window).darker(115);
    QColor textColor = pal.color(QPalette::ButtonText);

    painter->fillRect(rect, bgColor);

    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    QFont boldFont = painter->font();
    boldFont.setBold(true);
    painter->setFont(boldFont);
    painter->setPen(textColor);

    bool hasSortIndicator = isSortIndicatorShown() && sortIndicatorSection() == logicalIndex;
    QRect textRect = hasSortIndicator ? rect.adjusted(4, 0, -16, 0) : rect;
    painter->drawText(textRect, Qt::AlignCenter, text);

    if (hasSortIndicator) {
        int arrowSize = 6;
        int centerY = rect.center().y();
        int arrowX = rect.right() - 10;
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(textColor);  // Same color as text
        
        QPolygon triangle;
        if (sortIndicatorOrder() == Qt::AscendingOrder) {
            triangle << QPoint(arrowX, centerY + arrowSize/2)
                     << QPoint(arrowX + arrowSize, centerY + arrowSize/2)
                     << QPoint(arrowX + arrowSize/2, centerY - arrowSize/2);
        } else {
            triangle << QPoint(arrowX, centerY - arrowSize/2)
                     << QPoint(arrowX + arrowSize, centerY - arrowSize/2)
                     << QPoint(arrowX + arrowSize/2, centerY + arrowSize/2);
        }
        painter->drawPolygon(triangle);
    }

    painter->restore();
}


CardListWidget::CardListWidget(QWidget *parent) : QListWidget(parent)
{
    setMouseTracking(true);
    setSpacing(1);
    setItemDelegate(new CardListDelegate(this));

    updateColorsFromPalette();
}

void CardListWidget::updateColorsFromPalette()
{
    const QPalette &pal = palette();
    m_itemBackground = pal.color(QPalette::AlternateBase);
    m_itemBackgroundHover = pal.color(QPalette::Mid);
    m_itemBackgroundSelected = pal.color(QPalette::Highlight);
    m_titleColor = pal.color(QPalette::Text);
    m_titleColorSelected = pal.color(QPalette::HighlightedText);
    m_subtitleColor = pal.color(QPalette::Text);
    m_subtitleColor.setAlpha(160);
    m_subtitleColorSelected = pal.color(QPalette::HighlightedText);
    m_subtitleColorSelected.setAlpha(200);
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

    buttonClear = new QPushButton("Clear");

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





TextEditConsole::TextEditConsole(QWidget* parent, int maxLines, bool noWrap, bool autoScroll) : QTextEdit(parent), cachedCursor(this->textCursor()), maxLines(maxLines), autoScroll(autoScroll), noWrap(noWrap)
{
    cachedCursor.movePosition(QTextCursor::End);

    if (noWrap)
        setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );
    else
        setWordWrapMode( QTextOption::WrapAnywhere );

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
        if (checked) {
            setLineWrapMode(QTextEdit::NoWrap);
        } else {
            setLineWrapMode(QTextEdit::WidgetWidth);
            setWordWrapMode(QTextOption::WrapAnywhere);
        }
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
    
    if (syncMode) {
        static QTextCharFormat plainFormat;
        if (!pendingFormatted.isEmpty() && pendingFormatted.last().format == plainFormat) {
            pendingFormatted.last().text += text;
        } else {
            pendingFormatted.append({text, plainFormat});
        }
        return;
    }

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
    QTextCharFormat fmt;
    styleFn(fmt);

    if (syncMode) {
        QMutexLocker locker(batchMutex);
        if (!pendingFormatted.isEmpty() && pendingFormatted.last().format == fmt) {
            pendingFormatted.last().text += text;
        } else {
            pendingFormatted.append({text, fmt});
        }
        return;
    }

    flushPendingText();

    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();

    cachedCursor.movePosition(QTextCursor::End);
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

void TextEditConsole::setSyncMode(bool enabled)
{
    syncMode = enabled;
    if (!enabled)
        flushAll();
}

void TextEditConsole::flushAll()
{
    QMutexLocker locker(batchMutex);
    
    if (!pendingText.isEmpty()) {
        pendingFormatted.append({pendingText, QTextCharFormat()});
        pendingText.clear();
    }
    
    if (pendingFormatted.isEmpty())
        return;

    QList<FormattedChunk> chunks = std::move(pendingFormatted);
    pendingFormatted.clear();
    locker.unlock();

    bool atBottom = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    cachedCursor.movePosition(QTextCursor::End);

    int count = 0;
    for (const auto &chunk : chunks) {
        cachedCursor.insertText(chunk.text, chunk.format);
        if (++count % 50 == 0)
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    auto doc = this->document();
    int currentLines = doc->blockCount();
    if (currentLines > maxLines * 1.5) {
        trimExcessLines();
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