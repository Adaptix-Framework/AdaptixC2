#pragma once

#include <QWidget>
#include <QVector>
#include <QMap>
#include <QSet>

class CodeEditor;
class SyntaxStyle;

class CodeFolding : public QWidget
{
Q_OBJECT

public:
    explicit CodeFolding(CodeEditor* parent = nullptr);

    QSize sizeHint() const override;

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

    void updateFoldingData();

    bool isFolded(int line) const;
    void toggleFold(int line);
    void foldAll();
    void unfoldAll();

    QSet<int> ifdefLines() const;

    struct IfdefRegion
    {
        int startLine;
        int endLine;
        bool isActive;
    };
    QVector<IfdefRegion> ifdefRegions() const;

Q_SIGNALS:
    void foldToggled(int line, bool folded);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    enum class FoldType { Brace, Comment, Preprocessor };

    struct FoldRegion
    {
        int startLine;
        int endLine;
        int indentLevel;
        FoldType type;
    };

    SyntaxStyle* m_syntaxStyle;
    CodeEditor* m_editor;
    QMap<int, bool> m_foldedLines;
    QVector<FoldRegion> m_regions;
    mutable QSet<int> m_ifdefLines;
    mutable QVector<IfdefRegion> m_ifdefRegions;
    int m_hoveredLine;

    QVector<FoldRegion> detectFoldRegions() const;
    void applyFolding();
};
