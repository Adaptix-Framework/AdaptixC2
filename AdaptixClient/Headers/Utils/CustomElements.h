#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>

class QTimer;
class QMutex;



class CardListWidget : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor itemBackground READ itemBackground WRITE setItemBackground)
    Q_PROPERTY(QColor itemBackgroundHover READ itemBackgroundHover WRITE setItemBackgroundHover)
    Q_PROPERTY(QColor itemBackgroundSelected READ itemBackgroundSelected WRITE setItemBackgroundSelected)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QColor titleColorSelected READ titleColorSelected WRITE setTitleColorSelected)
    Q_PROPERTY(QColor subtitleColor READ subtitleColor WRITE setSubtitleColor)
    Q_PROPERTY(QColor subtitleColorSelected READ subtitleColorSelected WRITE setSubtitleColorSelected)

    QColor m_itemBackground = QColor(42, 42, 42);
    QColor m_itemBackgroundHover = QColor(50, 50, 50);
    QColor m_itemBackgroundSelected = QColor(11, 89, 45);
    QColor m_titleColor = QColor(190, 190, 190);
    QColor m_titleColorSelected = QColor(200, 200, 200);
    QColor m_subtitleColor = QColor(140, 140, 140);
    QColor m_subtitleColorSelected = QColor(180, 180, 180);

public:
    enum DataRole { TitleRole = Qt::UserRole, TextRole = Qt::UserRole + 1 };

    explicit CardListWidget(QWidget *parent = nullptr);

    void addCard(const QString &title, const QString &text);

    QColor itemBackground() const { return m_itemBackground; }
    void setItemBackground(const QColor &color) { m_itemBackground = color; }

    QColor itemBackgroundHover() const { return m_itemBackgroundHover; }
    void setItemBackgroundHover(const QColor &color) { m_itemBackgroundHover = color; }

    QColor itemBackgroundSelected() const { return m_itemBackgroundSelected; }
    void setItemBackgroundSelected(const QColor &color) { m_itemBackgroundSelected = color; }

    QColor titleColor() const { return m_titleColor; }
    void setTitleColor(const QColor &color) { m_titleColor = color; }

    QColor titleColorSelected() const { return m_titleColorSelected; }
    void setTitleColorSelected(const QColor &color) { m_titleColorSelected = color; }

    QColor subtitleColor() const { return m_subtitleColor; }
    void setSubtitleColor(const QColor &color) { m_subtitleColor = color; }

    QColor subtitleColorSelected() const { return m_subtitleColorSelected; }
    void setSubtitleColorSelected(const QColor &color) { m_subtitleColorSelected = color; }
};

class CardListDelegate : public QStyledItemDelegate
{
public:
    explicit CardListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};




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
