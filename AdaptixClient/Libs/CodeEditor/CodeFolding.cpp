#include <CodeFolding.h>
#include <CodeEditor.h>
#include <SyntaxStyle.h>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QPolygon>
#include <QRegularExpression>

CodeFolding::CodeFolding(CodeEditor* parent) :
    QWidget(parent),
    m_syntaxStyle(nullptr),
    m_editor(parent),
    m_foldedLines(),
    m_regions(),
    m_hoveredLine(-1)
{
    setMouseTracking(true);
}

QSize CodeFolding::sizeHint() const
{
    return {14, 0};
}

void CodeFolding::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;
}

SyntaxStyle* CodeFolding::syntaxStyle() const
{
    return m_syntaxStyle;
}

void CodeFolding::updateFoldingData()
{
    static bool updating = false;
    if (updating)
        return;

    updating = true;

    m_regions = detectFoldRegions();
    applyFolding();
    update();

    updating = false;
}

bool CodeFolding::isFolded(int line) const
{
    return m_foldedLines.value(line, false);
}

void CodeFolding::toggleFold(int line)
{
    m_foldedLines[line] = !m_foldedLines.value(line, false);
    applyFolding();
    update();
    Q_EMIT foldToggled(line, m_foldedLines[line]);
}

void CodeFolding::foldAll()
{
    for (auto& region : m_regions)
        m_foldedLines[region.startLine] = true;
    applyFolding();
    update();
}

void CodeFolding::unfoldAll()
{
    m_foldedLines.clear();

    auto doc = m_editor->document();
    for (int i = 0; i < doc->blockCount(); ++i) {
        doc->findBlockByNumber(i).setVisible(true);
    }
    doc->markContentsDirty(0, doc->characterCount());
    m_editor->viewport()->update();
    update();
}

QSet<int> CodeFolding::ifdefLines() const
{
    return m_ifdefLines;
}

QVector<CodeFolding::IfdefRegion> CodeFolding::ifdefRegions() const
{
    return m_ifdefRegions;
}

QVector<CodeFolding::FoldRegion> CodeFolding::detectFoldRegions() const
{
    QVector<FoldRegion> regions;
    auto doc = m_editor->document();
    m_ifdefLines.clear();
    m_ifdefRegions.clear();

    QSet<QString> defines;
    QRegularExpression definePattern(R"(^\s*#\s*define\s+(\w+))");
    QTextBlock blk = doc->begin();
    while (blk.isValid()) {
        auto m = definePattern.match(blk.text().trimmed());
        if (m.hasMatch())
            defines.insert(m.captured(1));
        blk = blk.next();
    }

    struct OpenBrace { int line; };
    QVector<OpenBrace> braceStack;

    struct IfdefInfo
    {
        int startLine;
        bool conditionTrue; // is the current branch active?
        bool elseReached;   // has #else been seen?
    };
    QVector<IfdefInfo> ifdefStack;

    QTextBlock block = doc->begin();
    int line = 0;
    bool inBlockComment = false;
    int commentStartLine = -1;

    QRegularExpression ifdefPattern(R"(^\s*#\s*(ifdef|ifndef|if)\s+(\w+))");
    QRegularExpression elsePattern(R"(^\s*#\s*(else|elif)\b)");
    QRegularExpression endifPattern(R"(^\s*#\s*endif\b)");

    while (block.isValid()) {
        QString text = block.text();
        QString trimmed = text.trimmed();
        int indent = 0;
        for (auto ch : text) {
            if (ch == ' ')
                indent++;
            else if (ch == '\t')
                indent += 4;
            else break;
        }

        if (!inBlockComment) {
            auto mIfdef = ifdefPattern.match(trimmed);
            if (mIfdef.hasMatch()) {
                QString directive = mIfdef.captured(1);
                QString macroName = mIfdef.captured(2);
                bool condTrue;
                if (directive == "ifdef")
                    condTrue = defines.contains(macroName);
                else if (directive == "ifndef")
                    condTrue = !defines.contains(macroName);
                else
                    condTrue = defines.contains(macroName);

                ifdefStack.append({line, condTrue, false});
            }
            else if (elsePattern.match(trimmed).hasMatch()) {
                if (!ifdefStack.isEmpty()) {
                    auto& info = ifdefStack.last();

                    if (line > info.startLine) {
                        bool sectionActive = info.conditionTrue;
                        regions.append(FoldRegion{info.startLine, line, indent, FoldType::Preprocessor});
                        if (!sectionActive)
                            for (int i = info.startLine + 1; i < line; ++i)
                                m_ifdefLines.insert(i);
                        m_ifdefRegions.append({info.startLine, line, sectionActive});
                    }

                    bool newCond = !info.conditionTrue;
                    info.startLine = line;
                    info.conditionTrue = newCond;
                    info.elseReached = true;
                }
            }
            else if (endifPattern.match(trimmed).hasMatch()) {
                if (!ifdefStack.isEmpty()) {
                    auto info = ifdefStack.last();
                    ifdefStack.removeLast();

                    if (line > info.startLine) {
                        bool sectionActive = info.conditionTrue;
                        regions.append(FoldRegion{info.startLine, line, indent, FoldType::Preprocessor});
                        if (!sectionActive)
                            for (int i = info.startLine + 1; i < line; ++i)
                                m_ifdefLines.insert(i);
                        m_ifdefRegions.append({info.startLine, line, sectionActive});
                    }
                }
            }
        }

        for (int i = 0; i < text.length(); ++i) {
            QChar ch = text[i];

            if (inBlockComment) {
                if (ch == '*' && i + 1 < text.length() && text[i + 1] == '/') {
                    inBlockComment = false;
                    if (line > commentStartLine)
                        regions.append(FoldRegion{commentStartLine, line, indent, FoldType::Comment});
                    commentStartLine = -1;
                    ++i;
                }
                continue;
            }

            if (ch == '"') {
                ++i;
                while (i < text.length() && text[i] != '"') {
                    if (text[i] == '\\') ++i;
                    ++i;
                }
                continue;
            }
            if (ch == '\'') {
                ++i;
                while (i < text.length() && text[i] != '\'') {
                    if (text[i] == '\\') ++i;
                    ++i;
                }
                continue;
            }
            if (ch == '/' && i + 1 < text.length()) {
                if (text[i + 1] == '/')
                    break;

                if (text[i + 1] == '*') {
                    inBlockComment = true;
                    commentStartLine = line;
                    ++i;
                    continue;
                }
            }

            if (ch == '{') {
                braceStack.append(OpenBrace{line});
            }
            else if (ch == '}') {
                if (!braceStack.isEmpty()) {
                    auto open = braceStack.last();
                    braceStack.removeLast();
                    if (line > open.line)
                        regions.append(FoldRegion{open.line, line, indent, FoldType::Brace});
                }
            }
        }

        block = block.next();
        ++line;
    }

    return regions;
}

void CodeFolding::applyFolding()
{
    auto doc = m_editor->document();

    for (int i = 0; i < doc->blockCount(); ++i) {
        auto block = doc->findBlockByNumber(i);
        block.setVisible(true);
        block.setUserState(0);
    }

    for (auto& region : m_regions) {
        if (!m_foldedLines.value(region.startLine, false))
            continue;

        switch (region.type)
        {
        case FoldType::Comment:
            doc->findBlockByNumber(region.startLine).setUserState(10);
            break;
        case FoldType::Preprocessor:
            doc->findBlockByNumber(region.startLine).setUserState(20);
            break;
        case FoldType::Brace:
        default:
            doc->findBlockByNumber(region.startLine).setUserState(1);
            break;
        }

        for (int i = region.startLine + 1; i < region.endLine; ++i) {
            auto block = doc->findBlockByNumber(i);
            if (block.isValid()) {
                block.setVisible(false);
                block.setUserState(2);
            }
        }

        doc->findBlockByNumber(region.endLine).setUserState(3);
    }

    doc->markContentsDirty(0, doc->characterCount());
    m_editor->viewport()->update();
}

void CodeFolding::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), m_syntaxStyle
        ? m_syntaxStyle->getFormat("Text").background().color()
        : Qt::white);

    if (!m_editor)
        return;

    QMap<int, FoldRegion> regionMap;
    for (auto& region : m_regions)
        regionMap.insert(region.startLine, region);

    auto blockNumber = m_editor->getFirstVisibleBlock();
    auto block = m_editor->document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    auto top = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).translated(0, -m_editor->verticalScrollBar()->value()).top();
    auto bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();

    QColor markerColor = m_syntaxStyle
        ? m_syntaxStyle->getFormat("FoldMarker").foreground().color()
        : Qt::gray;
    QColor markerBgColor = m_syntaxStyle
        ? m_syntaxStyle->getFormat("FoldMarker").background().color()
        : Qt::lightGray;

    painter.setRenderHint(QPainter::Antialiasing);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (regionMap.contains(blockNumber)) {
                bool folded = m_foldedLines.value(blockNumber, false);
                bool hovered = (m_hoveredLine == blockNumber);

                int h = bottom - top;
                int sz = qMin(h - 2, 8);
                if (sz < 4) sz = 4;

                int cx = sizeHint().width() / 2;
                int cy = top + h / 2;

                QPolygon triangle;
                if (folded)
                    triangle << QPoint(cx - sz/2, cy - sz/2) << QPoint(cx + sz/2, cy) << QPoint(cx - sz/2, cy + sz/2);
                else
                    triangle << QPoint(cx - sz/2, cy - sz/4) << QPoint(cx + sz/2, cy - sz/4) << QPoint(cx, cy + sz/2);

                if (hovered) {
                    painter.setBrush(markerColor);
                    painter.setPen(Qt::NoPen);
                }
                else {
                    painter.setBrush(markerBgColor);
                    painter.setPen(markerColor);
                }
                painter.drawPolygon(triangle);
            }
        }

        block = block.next();
        if (!block.isValid())
            break;

        top = bottom;
        bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void CodeFolding::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    QMap<int, FoldRegion> regionMap;
    for (auto& region : m_regions)
        regionMap.insert(region.startLine, region);

    auto blockNumber = m_editor->getFirstVisibleBlock();
    auto block = m_editor->document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    auto top = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).translated(0, -m_editor->verticalScrollBar()->value()).top();
    auto bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();

    int y = event->pos().y();

    while (block.isValid() && top <= y) {
        if (y >= top && y < bottom && regionMap.contains(blockNumber)) {
            toggleFold(blockNumber);
            return;
        }

        block = block.next();
        if (!block.isValid())
            break;

        top = bottom;
        bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void CodeFolding::mouseMoveEvent(QMouseEvent* event)
{
    QMap<int, FoldRegion> regionMap;
    for (auto& region : m_regions)
        regionMap.insert(region.startLine, region);

    auto blockNumber = m_editor->getFirstVisibleBlock();
    auto block = m_editor->document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    auto top = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).translated(0, -m_editor->verticalScrollBar()->value()).top();
    auto bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();

    int y = event->pos().y();
    int newHovered = -1;

    while (block.isValid() && top <= y) {
        if (y >= top && y < bottom) {
            if (regionMap.contains(blockNumber))
                newHovered = blockNumber;
            break;
        }

        block = block.next();
        if (!block.isValid())
            break;

        top = bottom;
        bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }

    if (newHovered != m_hoveredLine) {
        m_hoveredLine = newHovered;
        update();
    }
}
