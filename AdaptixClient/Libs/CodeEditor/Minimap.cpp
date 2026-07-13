#include <Minimap.h>
#include <QPainter>
#include <QPaintEvent>
#include <QTextDocument>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>

Minimap::Minimap(QTextDocument* document, QScrollBar* scrollBar, QWidget* parent) : QWidget(parent), m_document(document), m_scrollBar(scrollBar), m_dragging(false), m_scale(0.15), m_viewportOffset(0)
{
    setFixedWidth(120);
    setMouseTracking(true);

    if (m_document)
        connect(m_document, &QTextDocument::contentsChanged, this, [this]() { update(); });

    if (m_scrollBar) {
        connect(m_scrollBar, &QScrollBar::valueChanged, this, [this]() { update(); });
        connect(m_scrollBar, &QScrollBar::rangeChanged, this, [this]() { update(); });
    }
}

void Minimap::setDocument(QTextDocument* doc)
{
    m_document = doc;
    update();
}

void Minimap::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor(0x1a, 0x1a, 0x2e);
    painter.fillRect(rect(), bgColor);

    drawMinimap(painter);
}

void Minimap::drawMinimap(QPainter& painter)
{
    if (!m_document || !m_scrollBar)
        return;

    QFont monoFont("monospace");
    monoFont.setPointSizeF(2.0);
    painter.setFont(monoFont);

    QFontMetrics fm(monoFont);
    int lineH = qMax(3, fm.height());
    int margin = 4;
    int contentWidth = width() - margin * 2;

    QColor textColor(0x80, 0x80, 0x90);
    QColor keywordColor(0x56, 0x9c, 0xd6);
    QColor stringColor(0xce, 0x91, 0x78);
    QColor commentColor(0x6a, 0x99, 0x55);
    QColor typeColor(0x4e, 0xc9, 0xb0);
    QColor numberColor(0xb5, 0xce, 0xa8);
    QColor bgColor(0x1a, 0x1a, 0x2e);
    QColor viewportColor(0x3d, 0xae, 0xe9);
    QColor viewportBorder(0x3d, 0xae, 0xe9);

    int totalDocLines = 0;
    int visibleStartLine = 0;
    int visibleEndLine = 0;

    QTextBlock block = m_document->begin();
    while (block.isValid())
    {
        if (block.isVisible())
            totalDocLines++;
        block = block.next();
    }

    if (totalDocLines == 0)
        return;

    double scrollRatio = 0;
    double visibleRatio = 1.0;
    if (m_scrollBar->maximum() > 0)
    {
        scrollRatio = (double)m_scrollBar->value() / (m_scrollBar->maximum() + m_scrollBar->pageStep());
        visibleRatio = (double)m_scrollBar->pageStep() / (m_scrollBar->maximum() + m_scrollBar->pageStep());
    }

    int minimapHeight = height() - margin * 2;
    double lineScale = (double)minimapHeight / totalDocLines;

    block = m_document->begin();
    int lineIndex = 0;
    int y = margin;

    while (block.isValid()) {
        if (!block.isVisible()) {
            block = block.next();
            continue;
        }

        int lineY = margin + (int)(lineIndex * lineScale);
        int lineHDraw = qMax(2, (int)lineScale);

        if (lineY > height())
            break;

        QString text = block.text();

        QColor lineColor = textColor;
        QString trimmed = text.trimmed();

        if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith("*"))
            lineColor = commentColor;
        else if (trimmed.startsWith("#") || trimmed.startsWith("var ") || trimmed.startsWith("let ") || trimmed.startsWith("const ") || trimmed.startsWith("function ") || trimmed.startsWith("ax.") || trimmed.startsWith("menu.") || trimmed.startsWith("form.") || trimmed.startsWith("event.") || trimmed.startsWith("cmd."))
            lineColor = keywordColor;
        else if (trimmed.startsWith("\"") || trimmed.startsWith("'") || trimmed.startsWith("`"))
            lineColor = stringColor;
        else if (trimmed.contains("class ") || trimmed.contains("struct ") || trimmed.contains("enum ") || trimmed.contains("typedef "))
            lineColor = typeColor;

        int indent = 0;
        for (auto ch : text) {
            if (ch == ' ') indent++;
            else if (ch == '\t') indent += 4;
            else break;
        }

        if (indent > 0) {
            int indentX = margin + (int)(indent * 0.5);
            if (indentX < width() - margin) {
                QColor indentColor(0x30, 0x30, 0x40);
                painter.setPen(indentColor);
                painter.drawLine(indentX, lineY, indentX, lineY + lineHDraw);
            }
        }

        int textLen = text.length();
        if (textLen > 0 && !trimmed.isEmpty()) {
            int blockWidth = qMin(contentWidth, (int)(textLen * 0.8));
            if (blockWidth > 1) {
                QColor fillColor = lineColor;
                fillColor.setAlpha(120);
                painter.setPen(Qt::NoPen);
                painter.setBrush(fillColor);
                painter.drawRect(margin + (int)(indent * 0.5), lineY, blockWidth, lineHDraw);
            }
        }

        lineIndex++;
        block = block.next();
    }

    int viewTop = margin + (int)(scrollRatio * minimapHeight);
    int viewHeight = qMax(20, (int)(visibleRatio * minimapHeight));
    viewHeight = qMin(viewHeight, minimapHeight - (viewTop - margin));

    QColor vpBg(viewportColor.red(), viewportColor.green(), viewportColor.blue(), 30);
    painter.setPen(Qt::NoPen);
    painter.setBrush(vpBg);
    painter.drawRoundedRect(0, viewTop, width(), viewHeight, 2, 2);

    QColor vpBorder(viewportColor.red(), viewportColor.green(), viewportColor.blue(), 100);
    painter.setPen(QPen(vpBorder, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(0, viewTop, width(), viewHeight, 2, 2);

    painter.setPen(QPen(viewportColor, 2));
    painter.drawLine(0, viewTop, 0, viewTop + viewHeight);
}

void Minimap::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        scrollToPosition(e->pos().y());
    }
}

void Minimap::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging)
        scrollToPosition(e->pos().y());
}

void Minimap::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}

void Minimap::scrollToPosition(int y)
{
    if (!m_scrollBar || !m_document)
        return;

    int margin = 4;
    int minimapHeight = height() - margin * 2;
    double ratio = qBound(0.0, (double)(y - margin) / minimapHeight, 1.0);
    int target = (int)(ratio * m_scrollBar->maximum());
    m_scrollBar->setValue(target);
}
