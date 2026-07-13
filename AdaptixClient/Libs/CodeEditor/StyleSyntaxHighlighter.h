#pragma once

#include <QSyntaxHighlighter>

class SyntaxStyle;

class StyleSyntaxHighlighter : public QSyntaxHighlighter
{
Q_OBJECT
    SyntaxStyle* m_syntaxStyle;

public:
    explicit StyleSyntaxHighlighter(QTextDocument* document = nullptr);

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;
};
