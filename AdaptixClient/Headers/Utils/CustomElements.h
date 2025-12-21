#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>

class QTimer;
class QMutex;

class ListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QLineEdit* editor = new QLineEdit(parent);
        editor->setContentsMargins(1, 1, 1, 1);
        return editor;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        return QSize(size.width(), size.height() + 10);
    }
};



class PaddingDelegate : public QStyledItemDelegate {
public:
    explicit PaddingDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem optFull = option;
        initStyleOption(&optFull, index);

        QVariant bgVar = index.data(Qt::BackgroundRole);
        bool hasBg = bgVar.isValid();
        QBrush bgBrush = hasBg ? bgVar.value<QBrush>() : QBrush();

        if (hasBg)
            painter->fillRect(optFull.rect, bgBrush);

        optFull.state &= ~QStyle::State_HasFocus;

        QStyledItemDelegate::paint(painter, optFull, index);
    }
};





class SpinTable : public QWidget {
Q_OBJECT
public:
    QGridLayout*  layout      = nullptr;
    QTableWidget* table       = nullptr;
    QPushButton*  buttonAdd   = nullptr;
    QPushButton*  buttonClear = nullptr;

    SpinTable(int rows, int clomuns, QWidget* parent);

    explicit SpinTable(QWidget* parent = nullptr) { SpinTable(0,0,parent); }
    ~SpinTable() override = default;
};





class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(const QString &label, QWidget *parent = nullptr) : QLabel(label, parent) {}

    Q_SIGNALS:
        void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            Q_EMIT clicked();
        }
        QLabel::mousePressEvent(event);
    }
};




class TextEditConsole : public QTextEdit {
Q_OBJECT
    QTextCursor cachedCursor;
    int  maxLines    = 50000;
    bool autoScroll  = false;
    bool noWrap      = true;
    int appendCount  = 0;

    QString pendingText;
    QTimer* batchTimer = nullptr;
    QMutex* batchMutex = nullptr;
    static const int BATCH_INTERVAL_MS = 100;
    static const int MAX_BATCH_SIZE = 64 * 1024;

    void trimExcessLines();
    void createContextMenu(const QPoint &pos);
    void setBufferSize(int size);

private Q_SLOTS:
    void flushPendingText();

public:
    explicit TextEditConsole(QWidget* parent = nullptr, int maxLines = 50000, bool noWrap = true, bool autoScroll = false);

    void appendPlain(const QString& text);
    void appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn);

    void appendColor(const QString& text, QColor color);
    void appendBold(const QString& text);
    void appendUnderline(const QString& text);
    void appendColorBold(const QString& text, QColor color);
    void appendColorUnderline(const QString& text, QColor color);

    void setMaxLines(int lines);
    void setAutoScrollEnabled(bool enabled);
    bool isAutoScrollEnabled() const;
    bool isNoWrapEnabled() const;

Q_SIGNALS:
    void ctx_find();
    void ctx_history();
};

#endif
