#include <CodeEditor.h>
#include <LineNumberArea.h>
#include <SyntaxStyle.h>
#include <StyleSyntaxHighlighter.h>
#include <FrameHighlight.h>
#include <CodeFolding.h>
#include <ErrorIndicator.h>
#include <Minimap.h>
#include <Utils/FontManager.h>

#include <QTextBlock>
#include <QPaintEvent>
#include <QPainter>
#include <QFontDatabase>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextCharFormat>
#include <QCursor>
#include <QCompleter>
#include <QAbstractItemView>
#include <QMimeData>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QStringListModel>
#include <QCryptographicHash>
#include <QTimer>

static QVector<QPair<QString, QString>> parentheses = {
    {"(", ")"},
    {"{", "}"},
    {"[", "]"},
    {"\"", "\""},
    {"'", "'"}
};

CodeEditor::CodeEditor(QWidget* parent) :
    QTextEdit(parent),
    m_highlighter(nullptr),
    m_syntaxStyle(nullptr),
    m_lineNumberArea(new LineNumberArea(this)),
    m_frameHighlight(new FrameHighlight(this)),
    m_codeFolding(new CodeFolding(this)),
    m_errorIndicator(new ErrorIndicator(this, this)),
    m_minimap(new Minimap(document(), verticalScrollBar(), this)),
    m_completer(nullptr),
    m_autoIndentation(true),
    m_autoParentheses(true),
    m_replaceTab(true),
    m_tabReplace(QString(4, ' ')),
    m_filePath(),
    m_savedHash(),
    m_currentHash(),
    m_hashDirty(true)
{
    initDocumentLayoutHandlers();
    initFont();
    performConnections();

    setSyntaxStyle(SyntaxStyle::defaultStyle());

    m_codeFolding->setVisible(false);
    m_minimap->setVisible(false);

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        initFont();
        updateLineNumberAreaWidth(0);
        updateExtraSelection();
        viewport()->update();
    });
}

bool CodeEditor::event(QEvent* e)
{
    bool result = QTextEdit::event(e);

    if (e->type() == QEvent::StyleChange || e->type() == QEvent::Polish || e->type() == QEvent::PolishRequest || e->type() == QEvent::ApplicationFontChange || e->type() == QEvent::FontChange) {
        if (m_syntaxStyle)
            applyEditorPalette();
    }

    return result;
}

void CodeEditor::applyEditorPalette()
{
    if (!m_syntaxStyle)
        return;

    auto bgColor = m_syntaxStyle->getFormat("Text").background().color();
    auto fgColor = m_syntaxStyle->getFormat("Text").foreground().color();
    auto selBg = m_syntaxStyle->getFormat("Selection").background().color();
    auto selFg = m_syntaxStyle->getFormat("Selection").foreground().color();

    QPalette pal;
    pal.setColor(QPalette::Base, bgColor);
    pal.setColor(QPalette::Text, fgColor);
    pal.setColor(QPalette::Highlight, selBg);
    pal.setColor(QPalette::HighlightedText, selFg);
    pal.setColor(QPalette::Window, bgColor);
    pal.setColor(QPalette::WindowText, fgColor);

    setPalette(pal);
    viewport()->setPalette(pal);

    QPalette vpPal = viewport()->palette();
    vpPal.setColor(QPalette::Base, bgColor);
    viewport()->setPalette(vpPal);
    viewport()->setAutoFillBackground(true);
}

void CodeEditor::initDocumentLayoutHandlers()
{
    document()->documentLayout()->registerHandler( FrameHighlight::type(), m_frameHighlight );
}

void CodeEditor::initFont()
{
    QFont fnt = FontManager::instance().appMonoFont();
    fnt.setFixedPitch(true);
    fnt.setStyleHint(QFont::Monospace);
    setFont(fnt);
    document()->setDefaultFont(fnt);
}

void CodeEditor::performConnections()
{
    connect( document(), &QTextDocument::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth );

    connect( document(), &QTextDocument::contentsChanged, this, [this]() {
            if (m_codeFolding->isVisible())
                m_codeFolding->updateFoldingData();
            m_hashDirty = true;
        }
    );
    connect( verticalScrollBar(), &QScrollBar::valueChanged, [this](int) {
            m_lineNumberArea->update();
            if (m_codeFolding->isVisible())
                m_codeFolding->update();
        }
    );
    connect( this, &QTextEdit::cursorPositionChanged, this, &CodeEditor::updateExtraSelection );
    connect( this, &QTextEdit::selectionChanged, this, &CodeEditor::onSelectionChanged );
    connect( m_codeFolding, &CodeFolding::foldToggled, [this](int, bool) {
            updateLineNumberAreaWidth(0);
            updateExtraSelection();
        }
    );
}

void CodeEditor::setHighlighter(StyleSyntaxHighlighter* highlighter)
{
    if (m_highlighter)
        m_highlighter->setDocument(nullptr);

    m_highlighter = highlighter;

    if (m_highlighter) {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
        m_highlighter->setDocument(document());
    }
}

StyleSyntaxHighlighter* CodeEditor::highlighter() const
{
    return m_highlighter;
}

void CodeEditor::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;

    m_frameHighlight->setSyntaxStyle(m_syntaxStyle);
    m_lineNumberArea->setSyntaxStyle(m_syntaxStyle);
    m_codeFolding->setSyntaxStyle(m_syntaxStyle);
    m_errorIndicator->setSyntaxStyle(m_syntaxStyle);

    if (m_highlighter)
        m_highlighter->setSyntaxStyle(m_syntaxStyle);

    updateStyle();
}

SyntaxStyle* CodeEditor::syntaxStyle() const
{
    return m_syntaxStyle;
}

void CodeEditor::updateStyle()
{
    if (m_highlighter)
        m_highlighter->rehighlight();

    applyEditorPalette();
    updateExtraSelection();
}

void CodeEditor::onSelectionChanged()
{
    auto selected = textCursor().selectedText();
    auto cursor = textCursor();

    if (cursor.isNull())
        return;

    cursor.movePosition(QTextCursor::MoveOperation::Left);
    cursor.select(QTextCursor::SelectionType::WordUnderCursor);

    QSignalBlocker blocker(this);
    m_frameHighlight->clear(cursor);

    if (selected.size() > 1 && cursor.selectedText() == selected) {
        auto backup = textCursor();
        handleSelectionQuery(cursor);
        setTextCursor(backup);
    }
}

void CodeEditor::handleSelectionQuery(QTextCursor cursor)
{
    auto searchIterator = cursor;
    searchIterator.movePosition(QTextCursor::Start);
    searchIterator = document()->find(cursor.selectedText(), searchIterator);
    while (searchIterator.hasSelection()) {
        m_frameHighlight->frame(searchIterator);
        searchIterator = document()->find(cursor.selectedText(), searchIterator);
    }
}

void CodeEditor::resizeEvent(QResizeEvent* e)
{
    QTextEdit::resizeEvent(e);
    updateLineGeometry();
}

void CodeEditor::updateLineGeometry()
{
    QRect cr = contentsRect();

    int foldingWidth = 0;
    if (m_codeFolding->isVisible())
        foldingWidth = m_codeFolding->sizeHint().width();

    int totalWidth = m_lineNumberArea->sizeHint().width();

    m_lineNumberArea->setGeometry( QRect(cr.left(), cr.top(), totalWidth, cr.height()) );

    if (m_codeFolding->isVisible())
        m_codeFolding->setGeometry( QRect(cr.left() + totalWidth - foldingWidth, cr.top(), foldingWidth, cr.height()) );

    if (m_minimap->isVisible()) {
        int minimapW = m_minimap->width();
        m_minimap->setGeometry( QRect(cr.right() - minimapW, cr.top(), minimapW, cr.height()) );
    }
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    int leftWidth = m_lineNumberArea->sizeHint().width();
    int rightWidth = m_minimap->isVisible() ? m_minimap->width() : 0;
    setViewportMargins(leftWidth, 0, rightWidth, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect)
{
    m_lineNumberArea->update( 0, rect.y(), m_lineNumberArea->sizeHint().width(), rect.height() );

    if (m_codeFolding->isVisible())
        m_codeFolding->update();

    updateLineGeometry();

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::updateExtraSelection()
{
    QList<QTextEdit::ExtraSelection> extra;

    highlightCurrentLine(extra);
    highlightParenthesis(extra);
    highlightIfdefBlocks(extra);

    auto errorSelections = m_errorIndicator->buildExtraSelections();
    extra.append(errorSelections);

    setExtraSelections(extra);
}

void CodeEditor::highlightIfdefBlocks(QList<QTextEdit::ExtraSelection>& extraSelection)
{
    if (!m_codeFolding->isVisible())
        return;

    auto ifdefLines = m_codeFolding->ifdefLines();
    if (ifdefLines.isEmpty())
        return;

    QColor textColor = m_syntaxStyle
        ? m_syntaxStyle->getFormat("Text").foreground().color()
        : QColor(0, 0, 0);
    QColor dimColor = textColor;
    dimColor.setAlpha(80);

    QTextCharFormat dimFormat;
    dimFormat.setForeground(dimColor);

    for (int line : ifdefLines)
    {
        auto block = document()->findBlockByNumber(line);
        if (!block.isValid() || !block.isVisible())
            continue;

        QTextEdit::ExtraSelection selection;
        selection.format = dimFormat;
        selection.cursor = QTextCursor(block);
        selection.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        extraSelection.append(selection);
    }
}

void CodeEditor::highlightParenthesis(QList<QTextEdit::ExtraSelection>& extraSelection)
{
    auto currentSymbol = charUnderCursor();
    auto prevSymbol = charUnderCursor(-1);

    for (auto& pair : parentheses)
    {
        int direction;
        QChar counterSymbol;
        QChar activeSymbol;
        auto position = textCursor().position();

        if (pair.first == currentSymbol) {
            direction = 1;
            counterSymbol = pair.second[0];
            activeSymbol = currentSymbol;
        }
        else if (pair.second == prevSymbol) {
            direction = -1;
            counterSymbol = pair.first[0];
            activeSymbol = prevSymbol;
            position--;
        }
        else {
            continue;
        }

        auto counter = 1;

        while (counter != 0 && position > 0 && position < (document()->characterCount() - 1)) {
            position += direction;
            auto character = document()->characterAt(position);
            if (character == activeSymbol)
                ++counter;
            else if (character == counterSymbol)
                --counter;
        }

        auto format = m_syntaxStyle->getFormat("Parentheses");

        if (counter == 0) {
            ExtraSelection selection{};

            auto directionEnum =
                direction < 0
                ? QTextCursor::MoveOperation::Left
                : QTextCursor::MoveOperation::Right;

            selection.format = format;
            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.movePosition( directionEnum, QTextCursor::MoveMode::MoveAnchor, std::abs(textCursor().position() - position) );
            selection.cursor.movePosition( QTextCursor::MoveOperation::Right, QTextCursor::MoveMode::KeepAnchor, 1 );

            extraSelection.append(selection);

            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.movePosition( directionEnum, QTextCursor::MoveMode::KeepAnchor, 1 );

            extraSelection.append(selection);
        }

        break;
    }
}

void CodeEditor::highlightCurrentLine(QList<QTextEdit::ExtraSelection>& extraSelection)
{
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection{};

        selection.format = m_syntaxStyle->getFormat("CurrentLine");
        selection.format.setForeground(QBrush());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        extraSelection.append(selection);
    }
}

void CodeEditor::paintEvent(QPaintEvent* e)
{
    if (m_syntaxStyle) {
        auto bgColor = m_syntaxStyle->getFormat("Text").background().color();
        auto pal = palette();
        if (pal.color(QPalette::Base) != bgColor)
            applyEditorPalette();
    }

    updateLineNumberArea(e->rect());
    QTextEdit::paintEvent(e);

    QPainter painter(viewport());

    {
        QFontMetrics fm(font());
        int charWidth = fm.horizontalAdvance(' ');
        int tabSize = m_tabReplace.size();
        if (tabSize <= 0)
            tabSize = 4;

        QColor guideColor = m_syntaxStyle
            ? m_syntaxStyle->getFormat("LineNumber").foreground().color()
            : Qt::gray;
        guideColor.setAlpha(40);

        QPen guidePen(guideColor);
        guidePen.setStyle(Qt::DotLine);
        painter.setPen(guidePen);

        auto blockNumber = getFirstVisibleBlock();
        auto block = document()->findBlockByNumber(blockNumber);
        if (!block.isValid())
            return;

        auto scrollY = verticalScrollBar()->value();

        while (block.isValid()) {
            auto top = (int)document()->documentLayout()->blockBoundingRect(block).translated(0, -scrollY).top();
            if (top > viewport()->height())
                break;

            if (block.isVisible()) {
                QString text = block.text();
                int indent = 0;
                for (auto ch : text) {
                    if (ch == ' ')
                        indent++;
                    else if (ch == '\t')
                        indent += tabSize;
                    else
                        break;
                }

                for (int level = tabSize; level <= indent; level += tabSize) {
                    int x = level * charWidth;
                    auto bottom = top + (int)document()->documentLayout()->blockBoundingRect(block).height();
                    painter.drawLine(x, top, x, bottom);
                }
            }

            block = block.next();
        }
    }

    if (!m_codeFolding->isVisible())
        return;

    QColor dotsColor = m_syntaxStyle
        ? m_syntaxStyle->getFormat("Comment").foreground().color()
        : QColor(0, 128, 0);
    QColor dotsBg = m_syntaxStyle
        ? m_syntaxStyle->getFormat("CurrentLine").background().color()
        : QColor(238, 238, 238);

    auto blockNumber = getFirstVisibleBlock();
    auto block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    auto top = (int)document()->documentLayout()->blockBoundingRect(block).translated(0, -verticalScrollBar()->value()).top();
    auto bottom = top + (int)document()->documentLayout()->blockBoundingRect(block).height();

    QFont dotsFont = font();
    painter.setFont(dotsFont);
    QFontMetrics fm(dotsFont);

    while (block.isValid() && top <= viewport()->height()) {
        if (block.isVisible() && bottom >= 0) {
            int state = block.userState();
            int blockHeight = bottom - top;

            if (state == 1) {
                QString text = block.text();
                int bracePos = text.lastIndexOf('{');
                if (bracePos < 0)
                    bracePos = text.length() - 1;

                int textX = fm.horizontalAdvance(text.left(bracePos + 1)) + 4;
                int dotsWidth = fm.horizontalAdvance("...") + 8;
                QRect dotsRect(textX, top, dotsWidth, blockHeight);
                painter.fillRect(dotsRect, dotsBg);
                painter.setPen(dotsColor);
                painter.drawText(dotsRect, Qt::AlignCenter, "...");
            }
            else if (state == 10) {
                QString text = block.text();
                int commentStart = text.indexOf("/*");
                if (commentStart >= 0) {
                    int prefixX = fm.horizontalAdvance(text.left(commentStart + 2)) + 2;
                    int dotsWidth = fm.horizontalAdvance(" ... ") + 6;
                    QRect dotsRect(prefixX, top, dotsWidth, blockHeight);
                    painter.fillRect(dotsRect, dotsBg);
                    painter.setPen(dotsColor);
                    painter.drawText(dotsRect, Qt::AlignCenter, " ... ");
                }
            }
            else if (state == 20) {
                QString text = block.text();
                int textLen = fm.horizontalAdvance(text) + 4;
                int dotsWidth = fm.horizontalAdvance(" ... #endif") + 8;
                QRect dotsRect(textLen, top, dotsWidth, blockHeight);
                painter.fillRect(dotsRect, dotsBg);
                painter.setPen(dotsColor);
                painter.drawText(dotsRect, Qt::AlignCenter, " ... #endif");
            }
        }

        block = block.next();
        if (!block.isValid())
            break;

        top = bottom;
        bottom = top + (int)document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

int CodeEditor::getFirstVisibleBlock()
{
    QTextCursor curs = QTextCursor(document());
    curs.movePosition(QTextCursor::Start);
    for (int i = 0; i < document()->blockCount(); ++i) {
        QTextBlock block = curs.block();

        QRect r1 = viewport()->geometry();
        QRect r2 = document()->documentLayout()->blockBoundingRect(block).translated( viewport()->geometry().x(), viewport()->geometry().y() - verticalScrollBar()->sliderPosition() ).toRect();

        if (r1.intersects(r2))
            return i;

        curs.movePosition(QTextCursor::NextBlock);
    }
    return 0;
}

bool CodeEditor::proceedCompleterBegin(QKeyEvent* e)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return true;
        default:
            break;
        }
    }

    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
    return !(!m_completer || !isShortcut);
}

void CodeEditor::proceedCompleterEnd(QKeyEvent* e)
{
    auto ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

    if (!m_completer || (ctrlOrShift && e->text().isEmpty()) || e->key() == Qt::Key_Delete)
        return;

    static QString eow(R"(~!@#$%^&*()_+{}|:"<>?,./;'[]\-=)");

    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
    auto completionPrefix = wordUnderCursor();

    if (!isShortcut) {
        QChar charAfter = charUnderCursor(0);
        if (charAfter.isLetter() || charAfter.isDigit()) {
            m_completer->popup()->hide();
            return;
        }
    }

    auto cursor = textCursor();
    auto blockText = cursor.block().text();
    int posInBlock = cursor.positionInBlock();
    QString textBeforeCursor = blockText.left(posInBlock);

    bool isIncludeContext = false;
    QRegularExpression includeRe(R"(#\s*include\s*[<"]([^<>"]*))");
    auto includeMatch = includeRe.match(textBeforeCursor);
    if (includeMatch.hasMatch()) {
        isIncludeContext = true;
        completionPrefix = includeMatch.captured(1);
    }

    if (!isShortcut && (e->text().isEmpty() || completionPrefix.length() < 1 || eow.contains(e->text().right(1)))) {
        if (!isIncludeContext) {
            m_completer->popup()->hide();
            return;
        }
    }

    if (isIncludeContext && m_completer) {
        QStringList fileCompletions;
        QString basePath = m_filePath.isEmpty() ? QDir::currentPath() : QFileInfo(m_filePath).absolutePath();
        QDir dir(basePath);

        QString pattern = completionPrefix + "*";
        auto entries = dir.entryInfoList({pattern}, QDir::Files, QDir::Name);
        for (auto& entry : entries) {
            if (entry.suffix() == "h" || entry.suffix() == "hpp" || entry.suffix() == "hh")
                fileCompletions.append(entry.fileName());
        }

        auto subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (auto& subdir : subdirs) {
            if (!completionPrefix.contains("/")) {
                fileCompletions.append(subdir + "/");
            }
            else {
                QDir subDir(dir.path() + "/" + completionPrefix.left(completionPrefix.lastIndexOf("/")));
                QString subPattern = completionPrefix.mid(completionPrefix.lastIndexOf("/") + 1) + "*";
                auto subEntries = subDir.entryInfoList({subPattern}, QDir::Files, QDir::Name);
                for (auto& entry : subEntries) {
                    if (entry.suffix() == "h" || entry.suffix() == "hpp")
                        fileCompletions.append(entry.fileName());
                }
            }
        }

        if (!fileCompletions.isEmpty()) {
            auto fileModel = new QStringListModel(fileCompletions, m_completer);
            m_completer->setModel(fileModel);
            m_completer->setCompletionPrefix(completionPrefix);
            auto cursRect = cursorRect();
            cursRect.setWidth( m_completer->popup()->sizeHintForColumn(0) + m_completer->popup()->verticalScrollBar()->sizeHint().width() );
            m_completer->complete(cursRect);
            return;
        }
    }

    if (completionPrefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    auto cursRect = cursorRect();
    cursRect.setWidth( m_completer->popup()->sizeHintForColumn(0) + m_completer->popup()->verticalScrollBar()->sizeHint().width() );

    m_completer->complete(cursRect);
}

void CodeEditor::keyPressEvent(QKeyEvent* e)
{
    const int indentUnit = m_tabReplace.size();

    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_D) {
        auto cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString lineText = cursor.selectedText();

        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText(QString(QChar::ParagraphSeparator) + lineText);

        cursor.movePosition(QTextCursor::StartOfBlock);
        setTextCursor(cursor);
        return;
    }

    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_X) {
        auto cursor = textCursor();
        if (!cursor.hasSelection()) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            setTextCursor(cursor);
            return;
        }
    }

    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Slash) {
        auto cursor = textCursor();

        if (cursor.hasSelection()) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            cursor.setPosition(start);
            int startLine = cursor.blockNumber();
            cursor.setPosition(end);
            int endLine = cursor.blockNumber();

            bool allCommented = true;
            for (int i = startLine; i <= endLine; ++i) {
                auto block = document()->findBlockByNumber(i);
                QString trimmed = block.text().trimmed();
                if (!trimmed.startsWith("//")) {
                    allCommented = false;
                    break;
                }
            }

            int minIndent = INT_MAX;
            for (int i = startLine; i <= endLine; ++i) {
                auto block = document()->findBlockByNumber(i);
                QString text = block.text();
                if (text.trimmed().isEmpty())
                    continue;

                int col = 0;
                while (col < text.length() && text[col].isSpace()) col++;
                minIndent = qMin(minIndent, col);
            }
            if (minIndent == INT_MAX)
                minIndent = 0;

            cursor.beginEditBlock();
            for (int i = startLine; i <= endLine; ++i) {
                auto block = document()->findBlockByNumber(i);
                QString text = block.text();
                if (text.trimmed().isEmpty())
                    continue;

                if (allCommented) {
                    int pos = text.indexOf("//");
                    if (pos >= 0) {
                        int removeLen = 2;
                        if (pos + 2 < text.length() && text[pos + 2] == ' ')
                            removeLen = 3;
                        cursor.setPosition(block.position() + pos);
                        cursor.setPosition(block.position() + pos + removeLen, QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                    }
                }
                else {
                    cursor.setPosition(block.position() + minIndent);
                    cursor.insertText("// ");
                }
            }
            cursor.endEditBlock();
        }
        else {
            auto block = cursor.block();
            QString text = block.text();
            QString trimmed = text.trimmed();

            cursor.beginEditBlock();
            if (trimmed.startsWith("//")) {
                int pos = text.indexOf("//");
                if (pos >= 0) {
                    int removeLen = 2;
                    if (pos + 2 < text.length() && text[pos + 2] == ' ')
                        removeLen = 3;
                    cursor.setPosition(block.position() + pos);
                    cursor.setPosition(block.position() + pos + removeLen, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
            }
            else {
                int firstNonSpace = 0;
                while (firstNonSpace < text.length() && text[firstNonSpace].isSpace())
                    firstNonSpace++;

                cursor.setPosition(block.position() + firstNonSpace);
                cursor.insertText("// ");
            }
            cursor.endEditBlock();
        }
        return;
    }

    auto completerSkip = proceedCompleterBegin(e);

    if (!completerSkip) {
        if (m_replaceTab && e->key() == Qt::Key_Tab && e->modifiers() == Qt::NoModifier) {
            insertPlainText(m_tabReplace);
            return;
        }

        int indentationLevel = getIndentationSpaces();

        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && charUnderCursor() == '}' && charUnderCursor(-1) == '{') {
            int charsBack = 0;
            insertPlainText("\n");

            if (m_replaceTab)
                insertPlainText(QString(indentationLevel + indentUnit, ' '));
            else
                insertPlainText(QString(indentationLevel / 4 + 1, '\t'));

            insertPlainText("\n");
            charsBack++;

            if (m_replaceTab) {
                insertPlainText(QString(indentationLevel, ' '));
                charsBack += indentationLevel;
            }
            else {
                insertPlainText(QString(indentationLevel / 4, '\t'));
                charsBack += indentationLevel / 4;
            }

            while (charsBack--)
                moveCursor(QTextCursor::MoveOperation::Left);
            return;
        }

        if (m_replaceTab && e->key() == Qt::Key_Backtab) {
            int removeCount = std::min(indentationLevel, indentUnit);

            auto cursor = textCursor();
            cursor.movePosition(QTextCursor::MoveOperation::StartOfLine);
            cursor.movePosition(QTextCursor::MoveOperation::Right, QTextCursor::MoveMode::KeepAnchor, removeCount);
            cursor.removeSelectedText();
            return;
        }

        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
            auto cursor = textCursor();
            auto blockText = cursor.block().text();

            int posInBlock = cursor.positionInBlock();
            QChar lastChar;
            for (int i = posInBlock - 1; i >= 0; --i) {
                if (!blockText[i].isSpace()) {
                    lastChar = blockText[i];
                    break;
                }
            }

            QChar nextChar;
            for (int i = posInBlock; i < blockText.length(); ++i) {
                if (!blockText[i].isSpace()) {
                    nextChar = blockText[i];
                    break;
                }
            }

            QTextEdit::keyPressEvent(e);

            int newIndent = indentationLevel;

            if (lastChar == '{' || lastChar == '(' || lastChar == '[')
                newIndent += indentUnit;
            else if (nextChar == '}')
                newIndent = qMax(0, newIndent - indentUnit);

            if (m_replaceTab)
                insertPlainText(QString(newIndent, ' '));
            else
                insertPlainText(QString(newIndent / 4, '\t'));

            performSyntaxCheck();
        }
        else {
            if (m_autoParentheses && !e->text().isEmpty()) {
                for (auto&& el : parentheses) {
                    if (el.first == e->text() && el.first != el.second) {
                        QTextEdit::keyPressEvent(e);
                        insertPlainText(el.second);
                        moveCursor(QTextCursor::MoveOperation::Left);
                        proceedCompleterEnd(e);
                        return;
                    }

                    if (el.second == e->text()) {
                        auto symbol = charUnderCursor();
                        if (symbol == el.second) {
                            moveCursor(QTextCursor::MoveOperation::Right);
                            proceedCompleterEnd(e);
                            return;
                        }
                    }

                    if (el.first == el.second && el.first == e->text()) {
                        auto symbol = charUnderCursor();
                        if (symbol == el.second) {
                            moveCursor(QTextCursor::MoveOperation::Right);
                            proceedCompleterEnd(e);
                            return;
                        }
                        else {
                            QTextEdit::keyPressEvent(e);
                            insertPlainText(el.second);
                            moveCursor(QTextCursor::MoveOperation::Left);
                            proceedCompleterEnd(e);
                            return;
                        }
                    }
                }
            }

            QTextEdit::keyPressEvent(e);
        }

        if (m_autoIndentation && e->text() == "}") {
            auto cursor = textCursor();
            auto blockText = cursor.block().text();
            QString trimmed = blockText.trimmed();
            if (trimmed == "}") {
                cursor.movePosition(QTextCursor::StartOfLine);
                cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
                int newIndent = qMax(0, indentationLevel - indentUnit);
                cursor.insertText(QString(newIndent, ' ') + "}");
            }
        }

        if (!e->text().isEmpty() && e->text().at(0).isPrint())
            performSyntaxCheck();
    }

    proceedCompleterEnd(e);
}

void CodeEditor::setAutoIndentation(bool enabled)
{
    m_autoIndentation = enabled;
}

bool CodeEditor::autoIndentation() const
{
    return m_autoIndentation;
}

void CodeEditor::setAutoParentheses(bool enabled)
{
    m_autoParentheses = enabled;
}

bool CodeEditor::autoParentheses() const
{
    return m_autoParentheses;
}

void CodeEditor::setTabReplace(bool enabled)
{
    m_replaceTab = enabled;
}

bool CodeEditor::tabReplace() const
{
    return m_replaceTab;
}

void CodeEditor::setTabReplaceSize(int val)
{
    m_tabReplace.clear();
    m_tabReplace.fill(' ', val);
}

int CodeEditor::tabReplaceSize() const
{
    return m_tabReplace.size();
}

void CodeEditor::setCompleter(QCompleter* completer)
{
    if (m_completer)
        disconnect(m_completer, nullptr, this, nullptr);

    m_completer = completer;

    if (!m_completer)
        return;

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);

    connect( m_completer, QOverload<const QString&>::of(&QCompleter::activated), this, &CodeEditor::insertCompletion );
}

QCompleter* CodeEditor::completer() const
{
    return m_completer;
}

void CodeEditor::focusInEvent(QFocusEvent* e)
{
    if (m_completer)
        m_completer->setWidget(this);
    QTextEdit::focusInEvent(e);
}

void CodeEditor::insertCompletion(QString s)
{
    if (m_completer->widget() != this)
        return;

    auto tc = textCursor();
    int end = tc.position();
    int start = end;

    while (start > 0) {
        QChar ch = document()->characterAt(start - 1);
        if (ch.isLetterOrNumber() || ch == '_' || ch == '.')
            --start;
        else
            break;
    }

    tc.setPosition(start);
    tc.setPosition(end, QTextCursor::KeepAnchor);
    tc.insertText(s);
    setTextCursor(tc);
}

QChar CodeEditor::charUnderCursor(int offset) const
{
    auto block = textCursor().blockNumber();
    auto index = textCursor().positionInBlock();
    auto text = document()->findBlockByNumber(block).text();

    index += offset;

    if (index < 0 || index >= text.size())
        return {};

    return text[index];
}

QString CodeEditor::wordUnderCursor() const
{
    auto tc = textCursor();
    int end = tc.position();
    int start = end;

    while (start > 0) {
        QChar ch = document()->characterAt(start - 1);
        if (ch.isLetterOrNumber() || ch == '_' || ch == '.')
            --start;
        else
            break;
    }
    while (end < document()->characterCount()) {
        QChar ch = document()->characterAt(end);
        if (ch.isLetterOrNumber() || ch == '_')
            ++end;
        else
            break;
    }

    if (start == end)
        return QString();

    tc.setPosition(start);
    tc.setPosition(end, QTextCursor::KeepAnchor);
    return tc.selectedText();
}

void CodeEditor::insertFromMimeData(const QMimeData* source)
{
    insertPlainText(source->text());
}

int CodeEditor::getIndentationSpaces()
{
    auto blockText = textCursor().block().text();
    int indentationLevel = 0;

    for (auto i = 0; i < blockText.size() && QString("\t ").contains(blockText[i]); ++i)
    {
        if (blockText[i] == ' ')
            indentationLevel++;
        else
            indentationLevel += tabStopDistance() / fontMetrics().averageCharWidth();
    }

    return indentationLevel;
}

void CodeEditor::setCodeFoldingEnabled(bool enabled)
{
    m_codeFolding->setVisible(enabled);
    if (enabled)
        m_codeFolding->updateFoldingData();
    updateLineNumberAreaWidth(0);
    updateLineGeometry();
}

bool CodeEditor::isCodeFoldingEnabled() const
{
    return m_codeFolding->isVisible();
}

CodeFolding* CodeEditor::codeFolding() const
{
    return m_codeFolding;
}

ErrorIndicator* CodeEditor::errorIndicator() const
{
    return m_errorIndicator;
}

QString CodeEditor::filePath() const
{
    return m_filePath;
}

void CodeEditor::setFilePath(const QString& path)
{
    m_filePath = path;
}

bool CodeEditor::isModified() const
{
    if (m_hashDirty)
        const_cast<CodeEditor*>(this)->updateContentHash();
    return m_savedHash != m_currentHash;
}

void CodeEditor::markSaved()
{
    updateContentHash();
    m_savedHash = m_currentHash;
}

void CodeEditor::updateContentHash()
{
    m_currentHash = QCryptographicHash::hash( document()->toPlainText().toUtf8(), QCryptographicHash::Md5 );
    m_hashDirty = false;
}

void CodeEditor::performSyntaxCheck()
{
    QVector<ErrorIndicator::ErrorInfo> errors;
    auto doc = document();

    int braceDepth = 0;
    int parenDepth = 0;
    int bracketDepth = 0;

    QTextBlock block = doc->begin();
    int line = 0;
    bool inBlockComment = false;

    while (block.isValid()) {
        QString text = block.text();

        bool inString = false;
        bool inChar = false;
        bool inLineComment = false;
        QChar stringChar;
        bool lineHasCode = false;

        for (int i = 0; i < text.length(); ++i) {
            QChar ch = text[i];

            if (inLineComment)
                break;

            if (inBlockComment) {
                if (ch == '*' && i + 1 < text.length() && text[i + 1] == '/') {
                    inBlockComment = false;
                    ++i;
                }
                continue;
            }

            if (inString) {
                if (ch == '\\' && i + 1 < text.length()) {
                    ++i;
                    continue;
                }
                if (ch == stringChar)
                    inString = false;
                continue;
            }

            if (inChar) {
                if (ch == '\\' && i + 1 < text.length()) {
                    ++i;
                    continue;
                }
                if (ch == '\'')
                    inChar = false;
                continue;
            }

            if (ch == '"') {
                inString = true;
                stringChar = '"';
                lineHasCode = true;
                
                continue;
            }
            if (ch == '\'') {
                inChar = true;
                lineHasCode = true;
                
                continue;
            }
            if (ch == '/' && i + 1 < text.length()) {
                if (text[i + 1] == '/') {
                    inLineComment = true;
                    continue;
                }
                if (text[i + 1] == '*') {
                    inBlockComment = true;
                    ++i;
                    continue;
                }
            }

            if (ch == '{') {
                braceDepth++;
                lineHasCode = true;
                
            }
            else if (ch == '}') {
                braceDepth--;
                lineHasCode = true;
                
                if (braceDepth < 0) {
                    errors.append({line + 1, i + 1, 1, "Unmatched closing brace '}'", false});
                    braceDepth = 0;
                }
            }
            else if (ch == '(') {
                parenDepth++;
                lineHasCode = true;
                
            }
            else if (ch == ')') {
                parenDepth--;
                lineHasCode = true;
                
                if (parenDepth < 0) {
                    errors.append({line + 1, i + 1, 1, "Unmatched closing parenthesis ')'", false});
                    parenDepth = 0;
                }
            }
            else if (ch == '[') {
                bracketDepth++;
                lineHasCode = true;
                
            }
            else if (ch == ']') {
                bracketDepth--;
                lineHasCode = true;
                
                if (bracketDepth < 0) {
                    errors.append({line + 1, i + 1, 1, "Unmatched closing bracket ']'", false});
                    bracketDepth = 0;
                }
            }
            else if (!ch.isSpace())
                lineHasCode = true;
        }

        if (lineHasCode && braceDepth == 0 && parenDepth == 0 && bracketDepth == 0) {
            int endPos = text.length();
            if (inLineComment) {
                for (int i = 0; i < text.length() - 1; ++i) {
                    if (text[i] == '/' && text[i + 1] == '/') {
                        endPos = i;
                        break;
                    }
                }
            }

            QChar lastChar;
            for (int i = endPos - 1; i >= 0; --i) {
                if (!text[i].isSpace()) {
                    lastChar = text[i];
                    break;
                }
            }

            if (!lastChar.isNull() && lastChar != ';' && lastChar != '{' && lastChar != '}' && lastChar != ',' && lastChar != ':' && lastChar != '\\' && lastChar != '(' && lastChar != '[') {
                QString codePart = text.left(endPos).trimmed();
                if (codePart.contains('=') || (codePart.contains('(') && codePart.contains(')')))
                    errors.append({line + 1, (int)text.length(), 1, "Possible missing semicolon", true});
            }
        }

        block = block.next();
        ++line;
    }

    if (braceDepth > 0)
        errors.append({line, 1, 1, QString("Unclosed brace '{' (depth: %1)").arg(braceDepth), false});
    if (parenDepth > 0)
        errors.append({line, 1, 1, QString("Unclosed parenthesis '(' (depth: %1)").arg(parenDepth), true});

    m_errorIndicator->setErrors(errors);
    updateExtraSelection();
}

void CodeEditor::setMinimapEnabled(bool enabled)
{
    m_minimap->setVisible(enabled);
    updateLineNumberAreaWidth(0);
    updateLineGeometry();
}

bool CodeEditor::isMinimapEnabled() const
{
    return m_minimap->isVisible();
}
