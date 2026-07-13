#pragma once

#include <QObject>
#include <QTextObjectInterface>

class SyntaxStyle;

class FrameHighlight : public QObject, public QTextObjectInterface
{
Q_OBJECT
Q_INTERFACES(QTextObjectInterface)

    SyntaxStyle* m_style;

public:
    enum Property {
        FramedString = 1
    };

    static int type();

    explicit FrameHighlight(QObject* parent = nullptr);

    QSizeF intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format) override;
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int posInDocument, const QTextFormat& format) override;

    void frame(QTextCursor cursor);
    void clear(QTextCursor cursor);

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;
};
