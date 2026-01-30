#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>
#include <QTextLayout>
#include <QToolTip>

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

class WrapAnywhereDelegate : public PaddingDelegate {
    static constexpr int maxLines = 5;
public:
    explicit WrapAnywhereDelegate(QObject* parent = nullptr) : PaddingDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Background (same as PaddingDelegate)
        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid())
            painter->fillRect(opt.rect, bgVar.value<QBrush>());

        opt.state &= ~QStyle::State_HasFocus;

        // Draw everything except text using style
        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();

        // Draw background/selection
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        // Draw text with WrapAnywhere (max 5 lines)
        painter->save();
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);

        QString text = opt.text;
        QFontMetrics fm(opt.font);
        int lineHeight = fm.height();
        int maxHeight = lineHeight * maxLines;

        // Truncate text if it exceeds maxLines
        QString displayText = text;
        QTextOption layoutOpt(Qt::Alignment(opt.displayAlignment));
        layoutOpt.setWrapMode(QTextOption::WrapAnywhere);
        QTextLayout layout(text, opt.font);
        layout.setTextOption(layoutOpt);
        layout.beginLayout();
        int lineCount = 0;
        int lastLineEnd = 0;
        while (lineCount < maxLines) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(textRect.width());
            lastLineEnd = line.textStart() + line.textLength();
            lineCount++;
        }
        layout.endLayout();

        bool truncated = (lastLineEnd < text.length());
        if (truncated) {
            displayText = text.left(lastLineEnd).trimmed() + "...";
        }

        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAnywhere);
        textOption.setAlignment(Qt::Alignment(opt.displayAlignment));

        if (opt.state & QStyle::State_Selected)
            painter->setPen(opt.palette.highlightedText().color());
        else {
            QVariant fgVar = index.data(Qt::ForegroundRole);
            if (fgVar.isValid())
                painter->setPen(fgVar.value<QColor>());
            else
                painter->setPen(opt.palette.text().color());
        }

        painter->setFont(opt.font);
        painter->drawText(textRect, displayText, textOption);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QString text = index.data(Qt::DisplayRole).toString();
        int width = opt.rect.width() > 0 ? opt.rect.width() - 8 : 100;

        QFontMetrics fm(opt.font);
        int lineHeight = fm.height();
        QRect bound = fm.boundingRect(QRect(0, 0, width, 10000), Qt::TextWrapAnywhere, text);

        int maxHeight = lineHeight * maxLines + 8;
        return QSize(opt.rect.width(), qMin(bound.height() + 8, maxHeight));
    }

    QString displayText(const QVariant& value, const QLocale& locale) const override {
        return PaddingDelegate::displayText(value, locale);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override {
        if (event->type() == QEvent::ToolTip) {
            QString text = index.data(Qt::DisplayRole).toString();
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            QFontMetrics fm(opt.font);
            int width = opt.rect.width() - 8;
            if (width <= 0) width = 100;

            QTextOption layoutOpt;
            layoutOpt.setWrapMode(QTextOption::WrapAnywhere);
            QTextLayout layout(text, opt.font);
            layout.setTextOption(layoutOpt);
            layout.beginLayout();
            int lineCount = 0;
            while (true) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(width);
                lineCount++;
            }
            layout.endLayout();

            if (lineCount > maxLines) {
                // Wrap text manually to limit tooltip width
                QFontMetrics tooltipFm(QToolTip::font());
                QString wrappedText;
                int maxWidth = 1000;
                int pos = 0;
                while (pos < text.length()) {
                    int lineEnd = text.indexOf('\n', pos);
                    if (lineEnd == -1) lineEnd = text.length();
                    QString line = text.mid(pos, lineEnd - pos);

                    while (!line.isEmpty()) {
                        QString chunk;
                        int i = 1;
                        while (i <= line.length() && tooltipFm.horizontalAdvance(line.left(i)) < maxWidth)
                            i++;
                        chunk = line.left(i - 1);
                        if (chunk.isEmpty()) chunk = line.left(1);
                        wrappedText += chunk.toHtmlEscaped() + "<br>";
                        line = line.mid(chunk.length());
                    }
                    pos = lineEnd + 1;
                }
                if (wrappedText.endsWith("<br>"))
                    wrappedText.chop(4);
                QToolTip::showText(event->globalPos(), wrappedText, view);
                return true;
            }
        }
        return PaddingDelegate::helpEvent(event, view, option, index);
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
    struct FormattedChunk {
        QString text;
        QTextCharFormat format;
    };

    QTextCursor cachedCursor;
    int  maxLines    = 50000;
    bool autoScroll  = false;
    bool noWrap      = true;
    bool syncMode    = false;
    int appendCount  = 0;

    QString pendingText;
    QList<FormattedChunk> pendingFormatted;
    QTimer* batchTimer = nullptr;
    QMutex* batchMutex = nullptr;
    static constexpr int BATCH_INTERVAL_MS = 100;
    static constexpr int MAX_BATCH_SIZE = 64 * 1024;

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

    void setSyncMode(bool enabled);
    void flushAll();

Q_SIGNALS:
    void ctx_find();
    void ctx_history();
};

#endif
