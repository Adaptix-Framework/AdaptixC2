#pragma once

#include <QWidget>

class CodeEditor;
class SyntaxStyle;

class LineNumberArea : public QWidget
{
Q_OBJECT
    SyntaxStyle* m_syntaxStyle;
    CodeEditor* m_editor;

public:
    explicit LineNumberArea(CodeEditor* parent = nullptr);

    QSize sizeHint() const override;

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

protected:
    void paintEvent(QPaintEvent* event) override;
};
