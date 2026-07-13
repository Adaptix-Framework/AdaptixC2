#include <StyleSyntaxHighlighter.h>

StyleSyntaxHighlighter::StyleSyntaxHighlighter(QTextDocument* document) : QSyntaxHighlighter(document), m_syntaxStyle(nullptr) {}

void StyleSyntaxHighlighter::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;
}

SyntaxStyle* StyleSyntaxHighlighter::syntaxStyle() const
{
    return m_syntaxStyle;
}
