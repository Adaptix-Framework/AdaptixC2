#include <FrameHighlight.h>
#include <SyntaxStyle.h>
#include <QFontMetrics>
#include <QPainter>
#include <QTextBlock>

int FrameHighlight::type()
{
    return QTextFormat::UserFormat + 1;
}

FrameHighlight::FrameHighlight(QObject* parent) : QObject(parent), m_style(nullptr){}

void FrameHighlight::setSyntaxStyle(SyntaxStyle* style)
{
    m_style = style;
}

SyntaxStyle* FrameHighlight::syntaxStyle() const
{
    return m_style;
}

QSizeF FrameHighlight::intrinsicSize(QTextDocument*, int, const QTextFormat&)
{
    return {0, 0};
}

void FrameHighlight::drawObject(QPainter* painter, const QRectF& rect, QTextDocument*, int, const QTextFormat& format)
{
    auto textCharFormat = reinterpret_cast<const QTextCharFormat&>(format);
    auto font = textCharFormat.font();
    QFontMetrics metrics(font);

    auto string = format.property(FramedString).toString();
    auto stringSize = metrics.boundingRect(string).size();

    QRectF drawRect(rect.topLeft(), stringSize);
    drawRect.moveTop(rect.top() - stringSize.height());
    drawRect.adjust(0, 4, 0, 4);

    painter->setPen(m_style->getFormat("Occurrences").background().color());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRoundedRect(drawRect, 4, 4);
}

void FrameHighlight::frame(QTextCursor cursor)
{
    QTextCharFormat format;
    format.setObjectType(type());
    format.setProperty(FramedString, cursor.selectedText());

    if (cursor.selectionEnd() > cursor.selectionStart())
        cursor.setPosition(cursor.selectionStart());
    else
        cursor.setPosition(cursor.selectionEnd());

    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format );
}

void FrameHighlight::clear(QTextCursor cursor)
{
    auto doc = cursor.document();

    for (auto blockIndex = 0; blockIndex < doc->blockCount(); ++blockIndex) {
        auto block = doc->findBlockByNumber(blockIndex);
        auto formats = block.textFormats();
        int offset = 0;

        for (auto& fmt : formats) {
            if (fmt.format.objectType() == type()) {
                cursor.setPosition(block.position() + fmt.start - offset);
                cursor.deleteChar();
                ++offset;
            }
        }
    }
}
