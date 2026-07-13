#include <LineNumberArea.h>
#include <SyntaxStyle.h>
#include <CodeEditor.h>
#include <CodeFolding.h>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>

LineNumberArea::LineNumberArea(CodeEditor* parent) : QWidget(parent), m_syntaxStyle(nullptr), m_editor(parent) {}

QSize LineNumberArea::sizeHint() const
{
    if (m_editor == nullptr)
        return QWidget::sizeHint();

    int digits = 1;
    int max = qMax(1, m_editor->document()->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 13 + m_editor->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    int foldWidth = 0;
    if (m_editor->isCodeFoldingEnabled() && m_editor->codeFolding())
        foldWidth = m_editor->codeFolding()->sizeHint().width();

    return {space + foldWidth, 0};
}

void LineNumberArea::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;
}

SyntaxStyle* LineNumberArea::syntaxStyle() const
{
    return m_syntaxStyle;
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    painter.fillRect( event->rect(), m_syntaxStyle->getFormat("Text").background().color() );

    auto blockNumber = m_editor->getFirstVisibleBlock();
    auto block = m_editor->document()->findBlockByNumber(blockNumber);
    auto top = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).translated(0, -m_editor->verticalScrollBar()->value()).top();
    auto bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();

    auto currentLine = m_syntaxStyle->getFormat("CurrentLineNumber").foreground().color();
    auto otherLines = m_syntaxStyle->getFormat("LineNumber").foreground().color();

    painter.setFont(m_editor->font());

    int foldWidth = 0;
    if (m_editor->isCodeFoldingEnabled() && m_editor->codeFolding())
        foldWidth = m_editor->codeFolding()->sizeHint().width();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            auto isCurrentLine = m_editor->textCursor().blockNumber() == blockNumber;
            painter.setPen(isCurrentLine ? currentLine : otherLines);

            painter.drawText( 0, top, sizeHint().width() - foldWidth - 5, m_editor->fontMetrics().height(), Qt::AlignRight, number );
        }

        block = block.next();
        if (!block.isValid())
            break;

        top = bottom;
        bottom = top + (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }
}
