#pragma once

#include <QTextEdit>

class QCompleter;
class QTimer;
class SyntaxStyle;
class StyleSyntaxHighlighter;
class LineNumberArea;
class FrameHighlight;
class CodeFolding;
class ErrorIndicator;
class Minimap;

class CodeEditor : public QTextEdit
{
Q_OBJECT
    StyleSyntaxHighlighter* m_highlighter;
    SyntaxStyle* m_syntaxStyle;
    LineNumberArea* m_lineNumberArea;
    FrameHighlight* m_frameHighlight;
    CodeFolding* m_codeFolding;
    ErrorIndicator* m_errorIndicator;
    Minimap* m_minimap;
    QCompleter* m_completer;

    bool m_autoIndentation;
    bool m_autoParentheses;
    bool m_replaceTab;
    QString m_tabReplace;
    QString m_filePath;

    QByteArray m_savedHash;
    QByteArray m_currentHash;
    bool m_hashDirty;

    QTimer* m_foldUpdateTimer = nullptr;

    void initDocumentLayoutHandlers();
    void initFont();
    void performConnections();
    void scheduleFoldingUpdate();
    void updateLineGeometry();
    void handleSelectionQuery(QTextCursor cursor);
    void highlightParenthesis(QList<QTextEdit::ExtraSelection>& extra);
    void highlightCurrentLine(QList<QTextEdit::ExtraSelection>& extra);
    void highlightIfdefBlocks(QList<QTextEdit::ExtraSelection>& extra);

    bool proceedCompleterBegin(QKeyEvent* e);
    void proceedCompleterEnd(QKeyEvent* e);

    QChar charUnderCursor(int offset = 0) const;
    QString wordUnderCursor() const;
    int getIndentationSpaces();

    void updateContentHash();
    void applyEditorPalette();

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    int getFirstVisibleBlock();

    void setHighlighter(StyleSyntaxHighlighter* highlighter);
    StyleSyntaxHighlighter* highlighter() const;

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

    void setCompleter(QCompleter* completer);
    QCompleter* completer() const;

    void setAutoParentheses(bool enabled);
    bool autoParentheses() const;

    void setTabReplace(bool enabled);
    bool tabReplace() const;

    void setTabReplaceSize(int size);
    int tabReplaceSize() const;

    void setAutoIndentation(bool enabled);
    bool autoIndentation() const;

    void setCodeFoldingEnabled(bool enabled);
    bool isCodeFoldingEnabled() const;

    CodeFolding* codeFolding() const;
    ErrorIndicator* errorIndicator() const;

    void setMinimapEnabled(bool enabled);
    bool isMinimapEnabled() const;

    QString filePath() const;
    void setFilePath(const QString& path);

    bool isLargeDocument() const;

    bool isModified() const;
    void markSaved();

public Q_SLOTS:
    void insertCompletion(QString s);
    void updateLineNumberAreaWidth(int);
    void updateLineNumberArea(const QRect& rect);
    void updateExtraSelection();
    void updateStyle();
    void onSelectionChanged();
    void performSyntaxCheck();

protected:
    void insertFromMimeData(const QMimeData* source) override;
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    bool event(QEvent* e) override;
};
