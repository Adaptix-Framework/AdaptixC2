#pragma once

#include <QWidget>

class QTextDocument;
class QScrollBar;

class Minimap : public QWidget
{
Q_OBJECT
    QTextDocument* m_document;
    QScrollBar* m_scrollBar;
    bool m_dragging;
    double m_scale;
    int m_viewportOffset;

    void drawMinimap(QPainter& painter);
    void scrollToPosition(int y);

public:
    explicit Minimap(QTextDocument* document, QScrollBar* scrollBar, QWidget* parent = nullptr);

    void setDocument(QTextDocument* doc);

Q_SIGNALS:
    void scrollRequested(int value);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
};
