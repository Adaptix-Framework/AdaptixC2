#include <Utils/CustomElements/TextEditConsole.h>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QTextBlock>
#include <QPainter>
#include <QKeyEvent>
#include <oclero/qlementine/widgets/Menu.hpp>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Settings.h>
#include <MainAdaptix.h>

ConsoleHighlighter::ConsoleHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {}

void ConsoleHighlighter::highlightBlock(const QString& text)
{
    Q_UNUSED(text)
    auto* data = dynamic_cast<ConsoleBlockData*>(currentBlockUserData());
    if (!data)
        return;

    for (const auto& range : data->formats)
        setFormat(range.start, range.length, range.format);
}


TextEditConsole::TextEditConsole(QWidget* parent, int maxLines, bool noWrap, bool autoScroll) : QPlainTextEdit(parent), cachedCursor(this->textCursor()), prependCursor(this->textCursor()), maxLines(maxLines), autoScroll(autoScroll), noWrap(noWrap)
{
    cachedCursor.movePosition(QTextCursor::End);
    prependCursor.movePosition(QTextCursor::Start);

    if (noWrap)
        setLineWrapMode(QPlainTextEdit::NoWrap);
    else
        setLineWrapMode(QPlainTextEdit::WidgetWidth);

    highlighter = new ConsoleHighlighter(this->document());

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &TextEditConsole::customContextMenuRequested, this, &TextEditConsole::createContextMenu);

    batchTimer = new QTimer(this);
    batchTimer->setSingleShot(true);
    batchTimer->setInterval(BATCH_INTERVAL_MS);
    connect(batchTimer, &QTimer::timeout, this, &TextEditConsole::flushPending);

    viewport()->installEventFilter(this);
    forceTransparentBase();
}

void TextEditConsole::forceTransparentBase()
{
    if (m_suppressPaletteGuard) return;
    m_suppressPaletteGuard = true;
    QPalette vp = viewport()->palette();
    if (vp.color(QPalette::Base).alpha() != 0) {
        vp.setColor(QPalette::Base, Qt::transparent);
        viewport()->setPalette(vp);
    }
    m_suppressPaletteGuard = false;
}

bool TextEditConsole::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == viewport() &&
        (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)) {
        forceTransparentBase();
        }
    return QPlainTextEdit::eventFilter(obj, event);
}

void TextEditConsole::createContextMenu(const QPoint &pos) {
    auto *menu = new oclero::qlementine::Menu(this);

    QAction *copyAction = menu->addAction("Copy         (Ctrl + C)");
    connect(copyAction, &QAction::triggered, this, [this]() { copy(); });

    QAction *selectAllAction = menu->addAction("Select All   (Ctrl + A)");
    connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    QAction *findAction = menu->addAction("Find         (Ctrl + F)");
    connect(findAction, &QAction::triggered, this, [this]() { Q_EMIT ctx_find(); });

    QAction *clearAction = menu->addAction("Clear        (Ctrl + L)");
    connect(clearAction, &QAction::triggered, this, [this]() { Q_EMIT ctx_clear(); });

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
        if (checked)
            setLineWrapMode(QPlainTextEdit::NoWrap);
        else
            setLineWrapMode(QPlainTextEdit::WidgetWidth);
    });

    QAction *autoScrollAction = menu->addAction("Auto scroll");
    autoScrollAction->setCheckable(true);
    autoScrollAction->setChecked(autoScroll);
    connect(autoScrollAction, &QAction::toggled, this, &TextEditConsole::setAutoScrollEnabled);

    QAction *bgImageAction = menu->addAction("Show background image");
    bgImageAction->setCheckable(true);
    bgImageAction->setChecked(showBgImage);
    connect(bgImageAction, &QAction::toggled, this, [this](bool checked) {
        showBgImage = checked;
        Q_EMIT ctx_bgToggled(checked);
    });

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

bool TextEditConsole::isShowBackgroundImage() const {
    return showBgImage;
}

void TextEditConsole::setShowBackgroundImage(const bool enabled) {
    showBgImage = enabled;
    Q_EMIT ctx_bgToggled(enabled);
}

void TextEditConsole::setConsoleBackground(const QColor& bgColor, const QString& imagePath, int dimming)
{
    m_bgColor = bgColor;
    m_bgDimming = qBound(0, dimming, 100);

    if (!imagePath.isEmpty()) {
        m_bgPixmap = QPixmap(imagePath);
        m_hasBgImage = !m_bgPixmap.isNull();
    } else {
        m_bgPixmap = QPixmap();
        m_hasBgImage = false;
    }

    updatePaletteBackground();
    viewport()->update();
}

void TextEditConsole::updatePaletteBackground()
{
    forceTransparentBase();
    viewport()->update();
}

void TextEditConsole::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter(viewport());
        const QRect rect = viewport()->rect();
        painter.fillRect(rect, m_bgColor);

        if (showBgImage && m_hasBgImage) {
            QPixmap scaled = m_bgPixmap.scaled(rect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (rect.width()  - scaled.width())  / 2;
            int y = (rect.height() - scaled.height()) / 2;
            painter.drawPixmap(x, y, scaled);

            QColor overlay = m_bgColor;
            overlay.setAlpha(qRound(m_bgDimming * 255.0 / 100.0));
            painter.fillRect(rect, overlay);
        }
    }
    QPlainTextEdit::paintEvent(event);
}

bool TextEditConsole::isNoWrapEnabled() const {
    return noWrap;
}

void TextEditConsole::appendChunk(const QString& text, const QTextCharFormat& fmt)
{
    QMutexLocker locker(&batchMutex);

    if (!pendingChunks.isEmpty() && pendingChunks.last().format == fmt) {
        pendingChunks.last().text += text;
    } else {
        pendingChunks.append({text, fmt});
    }
    pendingSize += text.size();

    if (syncMode)
        return;

    if (pendingSize >= MAX_BATCH_SIZE) {
        QList<FormattedChunk> chunks = std::move(pendingChunks);
        pendingChunks.clear();
        pendingSize = 0;
        locker.unlock();
        insertChunks(chunks);
    } else if (!batchTimer->isActive()) {
        batchTimer->start();
    }
}

void TextEditConsole::insertChunks(const QList<FormattedChunk>& chunks)
{
    bool atBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 4;

    highlighter->blockSignals(true);

    QTextCursor& cursor = prependMode ? prependCursor : cachedCursor;
    if (!prependMode)
        cursor.movePosition(QTextCursor::End);

    for (const auto& chunk : chunks) {
        if (chunk.text.isEmpty())
            continue;

        bool hasFormat = chunk.format != QTextCharFormat();

        QString normalizedText = chunk.text;
        normalizedText.replace("\r\n", "\n");
        normalizedText.remove('\r');

        QStringList lines = normalizedText.split('\n');

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];

            if (!line.isEmpty()) {
                QTextBlock block = cursor.block();
                int posInBlock = cursor.positionInBlock();

                cursor.insertText(line);

                if (hasFormat) {
                    auto* data = dynamic_cast<ConsoleBlockData*>(block.userData());
                    if (!data) {
                        data = new ConsoleBlockData();
                        block.setUserData(data);
                    }
                    data->formats.append({posInBlock, static_cast<int>(line.length()), chunk.format});
                }
            }

            if (i < lines.size() - 1) {
                cursor.insertBlock();
            }
        }
    }

    highlighter->blockSignals(false);
    if (!suppressHighlight)
        highlighter->rehighlight();

    if (prependMode)
        return;

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

void TextEditConsole::beginPrepend()
{
    flushAll();
    prependMode = true;
    setSyncMode(true);
    prependCursor = QTextCursor(document());
    prependCursor.movePosition(QTextCursor::Start);
}

void TextEditConsole::endPrepend()
{
    setSyncMode(false);
    prependMode = false;
    historyBlockCount = document()->blockCount();
    cachedCursor = QTextCursor(document());
    cachedCursor.movePosition(QTextCursor::End);
}

bool TextEditConsole::isPrependMode() const { return prependMode; }

void TextEditConsole::appendPlain(const QString& text)
{
    appendChunk(text, QTextCharFormat());
}

void TextEditConsole::flushPending()
{
    QMutexLocker locker(&batchMutex);
    if (pendingChunks.isEmpty())
        return;

    QList<FormattedChunk> chunks = std::move(pendingChunks);
    pendingChunks.clear();
    pendingSize = 0;
    locker.unlock();

    insertChunks(chunks);
}

void TextEditConsole::appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn)
{
    QTextCharFormat fmt;
    styleFn(fmt);
    appendChunk(text, fmt);
}

void TextEditConsole::setSyncMode(bool enabled)
{
    syncMode = enabled;
    if (!enabled)
        flushAll();
}

void TextEditConsole::setBulkInsertMode(bool enabled)
{
    if (suppressHighlight == enabled)
        return;
    suppressHighlight = enabled;
    if (!enabled && highlighter) {
        flushAll();
        highlighter->rehighlight();
    }
}

void TextEditConsole::resetHistoryCount()
{
    historyBlockCount = 0;
}

void TextEditConsole::flushAll()
{
    QMutexLocker locker(&batchMutex);

    if (pendingChunks.isEmpty()) {
        pendingSize = 0;
        return;
    }

    QList<FormattedChunk> chunks = std::move(pendingChunks);
    pendingChunks.clear();
    pendingSize = 0;
    locker.unlock();

    insertChunks(chunks);
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

    if (historyBlockCount > 0) {
        int liveBlockCount = blockCount - historyBlockCount;
        if (liveBlockCount <= maxLines)
            return;

        int removeCount = liveBlockCount - maxLines;
        QTextBlock startBlock = doc->findBlockByNumber(historyBlockCount);
        QTextBlock removeToBlock = doc->findBlockByNumber(historyBlockCount + removeCount);
        if (!startBlock.isValid() || !removeToBlock.isValid())
            return;

        QTextCursor c(doc);
        c.setPosition(startBlock.position());
        c.setPosition(removeToBlock.position(), QTextCursor::KeepAnchor);
        c.removeSelectedText();
    } else {
        if (blockCount <= maxLines)
            return;

        QTextCursor c(doc);
        c.movePosition(QTextCursor::Start);

        QTextBlock keepFromBlock = doc->findBlockByNumber(blockCount - maxLines);
        if (keepFromBlock.isValid()) {
            c.movePosition(QTextCursor::Start);
            c.setPosition(keepFromBlock.position(), QTextCursor::KeepAnchor);
            c.removeSelectedText();
        }
    }
}
